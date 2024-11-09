/*
 * squashfs image editor
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "imgeditor.h"
#include "structure.h"
#include "string_helper.h"

typedef uint32_t u32;
typedef uint64_t u64;
#include "squashfs_fs.h"

#define SQUASHFS_MAGIC					0x73717368

static const uint8_t squashfs_magic[4] = {
	(SQUASHFS_MAGIC >>  0) & 0xff,
	(SQUASHFS_MAGIC >>  8) & 0xff,
	(SQUASHFS_MAGIC >> 16) & 0xff,
	(SQUASHFS_MAGIC >> 24) & 0xff,
};

#define PRINT_LEVEL0					"%-30s: "
#define PRINT_LEVEL1					"    %-26s: "
#define PRINT_LEVEL3					"        %-22s: "

static void structure_print_squashfs_sb_flags(const char *print_name_fmt,
					      const char *name,
					      const void *data,
					      size_t sz)
{
	#define SQUASHFS_SB_FLAG_BIT(type) { 1 << SQUASHFS_##type, #type }
	static const struct structure_bit_descriptor flags[] = {
		SQUASHFS_SB_FLAG_BIT(NOI),
		SQUASHFS_SB_FLAG_BIT(NOD),
		SQUASHFS_SB_FLAG_BIT(NOF),
		SQUASHFS_SB_FLAG_BIT(NO_FRAG),
		SQUASHFS_SB_FLAG_BIT(ALWAYS_FRAG),
		SQUASHFS_SB_FLAG_BIT(DUPLICATE),
		SQUASHFS_SB_FLAG_BIT(EXPORT),
		SQUASHFS_SB_FLAG_BIT(COMP_OPT),
		{ 0, NULL },
	};
	#undef SQUASHFS_SB_FLAG_BIT

	structure_item_print_bit_flags(print_name_fmt, name, data, sz, flags);
};

static void structure_print_squashfs_sb_compression(const char *print_name_fmt,
						    const char *name,
						    const void *data, size_t sz)
{
	#define SQUASHFS_SB_COMPRESSION(type) { type##_COMPRESSION, #type }
	static const struct structure_bit_descriptor compressions[] = {
		SQUASHFS_SB_COMPRESSION(ZLIB),
		SQUASHFS_SB_COMPRESSION(LZMA),
		SQUASHFS_SB_COMPRESSION(LZO),
		SQUASHFS_SB_COMPRESSION(XZ),
		SQUASHFS_SB_COMPRESSION(LZ4),
		{ 0, NULL },
	};
	#undef SQUASHFS_SB_COMPRESSION

	structure_item_print_enum(print_name_fmt, name, data, sz, 0, 0,
				  compressions);
}

static const struct structure_item st_squashfs_sblock[] = {
	STRUCTURE_ITEM(struct squashfs_super_block, inodes,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, mkfs_time,			structure_item_print_unix_epoch),
	STRUCTURE_ITEM(struct squashfs_super_block, block_size,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, fragments,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, compression,		structure_print_squashfs_sb_compression),
	STRUCTURE_ITEM(struct squashfs_super_block, block_log,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, flags,			structure_print_squashfs_sb_flags),
	STRUCTURE_ITEM(struct squashfs_super_block, no_ids,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, s_major,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, s_minor,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, root_inode,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, bytes_used,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, id_table_start,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, xattr_id_table_start,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, inode_table_start,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, directory_table_start,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, fragment_table_start,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct squashfs_super_block, lookup_table_start,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

struct squashfs_point {
	void					*buf;
	size_t					size;

	/* the point's offset in the fd file */
	uint64_t				global_offset;
};

struct squashfs_ppoint {
	struct squashfs_point			*arrays;
	size_t					array_count;
};

static void squashfs_ppoint_free(struct squashfs_ppoint *pp)
{
	if (!pp->arrays || pp->array_count == 0)
		return;

	for (size_t i = 0; i < pp->array_count; i++) {
		struct squashfs_point *p = &pp->arrays[i];

		if (p->buf)
			free(p->buf);
	}

	free(pp->arrays);
	pp->arrays = NULL;
	pp->array_count = 0;
}

struct squashfs_private_data {
	struct squashfs_super_block		sb;

	void					*inode_table;
	size_t					inode_table_size;

	void					*dir_table;
	size_t					dir_table_size;

	struct squashfs_ppoint			uids;
	struct squashfs_ppoint			fragments;
};

/* In constrast to data blocks, every metadata block is preceded by a single,
 * 16 bit unsigned integer. This integer holds the on-disk size of the block
 * that follows. The MSB is set if the block is stored uncompressed.
 * Whenever a metadata block is referenced, the position of this integer is
 * given.
 *
 * Example: 04 80 EB 03 00 00
 * => 0x8004 0x000003EB
 * meta data size is 0x04 and uncompressed
 */
static int squashfs_read_metablock(struct squashfs_private_data *squashfs,
				  int fd, uint64_t offset, void *buf, size_t bufsz,
				  size_t *ret_metadata_sz, uint64_t *ret_next_offset)
{
	__le16 header = 0;
	size_t sz;
	int ret;

	fileseek(fd, offset);
	ret = fileread(fd, &header, sizeof(header));
	if (ret < 0)
		return ret;

	if (!(le16_to_cpu(header) & SQUASHFS_COMPRESSED_BIT)) {
		fprintf(stderr, "Error: meta block in %" PRIu64 " was compressed. "
				"we not support it now\n",
			offset);
		return -1;
	}

	sz = SQUASHFS_COMPRESSED_SIZE(le16_to_cpu(header));
	if (sz > SQUASHFS_METADATA_SIZE) {
		fprintf(stderr, "Error: meta block in %" PRIu64 " was overflow."
				" (header: 0x%04x)\n",
			offset, le16_to_cpu(header));
		return -1;
	}

	if (ret_next_offset)
		*ret_next_offset = offset + sz + sizeof(header);

	if (sz > bufsz)
		sz = bufsz;

	*ret_metadata_sz = sz;
	ret = fileread(fd, buf, sz);
	if (ret < 0) {
		fprintf(stderr, "Error: read meta block in %" PRIu64 " failed\n",
			offset);
		return ret;
	}

	return 0;
}

static void *squashfs_alloc_metablocks(struct squashfs_private_data *squashfs,
				       int fd, uint64_t offset,
				       uint64_t limit_offset,
				       size_t limit_max_blocks,
				       size_t *ret_metadata_sz)
{
	size_t block_count = 0, total_bytes = 0, bufsz = SQUASHFS_METADATA_SIZE;
	void *buf = malloc(bufsz);

	if (!buf) {
		*ret_metadata_sz = 0;
		return buf;
	}

	while (1) {
		size_t chunk_sz = SQUASHFS_METADATA_SIZE;
		void *newbuf;
		int ret;

		if (limit_max_blocks > 0 && block_count >= limit_max_blocks)
			break;

		if (limit_offset > 0) {
			size_t remain = limit_offset - offset;

			if (offset >= limit_offset)
				break;

			if (remain < chunk_sz)
				chunk_sz = remain;
		}

		newbuf = realloc(buf, bufsz);
		if (!newbuf) {
			fprintf(stderr, "Error: alloc buf for meta blocks failed\n");
			*ret_metadata_sz = 0;
			free(buf);
			return NULL;
		}

		buf = newbuf;

		ret = squashfs_read_metablock(squashfs, fd, offset,
					      buf + total_bytes,
					      chunk_sz,
					      &chunk_sz, &offset);
		if (ret < 0) {
			*ret_metadata_sz = 0;
			free(buf);
			return NULL;
		}

		bufsz += SQUASHFS_METADATA_SIZE;
		total_bytes += chunk_sz;
		block_count++;
	}

	*ret_metadata_sz = total_bytes;
	return buf;
}

static int squashfs_read_ppoint(struct squashfs_private_data *squashfs,
				int fd, uint64_t ppoffset,  size_t count,
				struct squashfs_ppoint *pp)
{
	int ret = 0;

	pp->array_count = count;

	pp->arrays = calloc(pp->array_count, sizeof(*pp->arrays));
	if (!pp->arrays) {
		fprintf(stderr, "Alloc %zu ppoint failed at offset "
			"0x%" PRIx64 "\n",
			count, ppoffset);
		return -1;
	}

	fileseek(fd, ppoffset);

	for (size_t i = 0; i < count; i++) {
		struct squashfs_point *p = &pp->arrays[i];
		uint64_t offset;

		ret = fileread(fd, &offset, sizeof(offset));
		if (ret < 0) {
			fprintf(stderr, "Read pp at offset %" PRIu64 "+%zu"
				"failed\n",
				ppoffset, i);
			break;
		}

		p->global_offset = le64_to_cpu(offset);

		p->buf = squashfs_alloc_metablocks(squashfs, fd, p->global_offset,
						   0, /* limit offset */
						   1, /* limit max blocks */
						   &p->size);
		if (!p->buf) {
			fprintf(stderr, "Read pp metablock at offset %"
				PRIu64 "+%zu failed\n",
				ppoffset, i);
			ret = -1;
			break;
		}
	}

	if (ret < 0) {
		for (size_t i = 0; i < count; i++) {
			struct squashfs_point *p = &pp->arrays[i];

			if (p->buf)
				free(p->buf);
		}
		free(pp->arrays);
		pp->array_count = 0;
		pp->arrays = NULL;
		return ret;
	}

	return ret;
}

static void squashfs_exit(void *private_data)
{
	struct squashfs_private_data *p = private_data;

	if (p->inode_table) {
		free(p->inode_table);
		p->inode_table = NULL;
		p->inode_table_size = 0;
	}

	if (p->dir_table) {
		free(p->dir_table);
		p->dir_table = NULL;
		p->dir_table_size = 0;
	}

	squashfs_ppoint_free(&p->uids);
	squashfs_ppoint_free(&p->fragments);
}

static int squashfs_detect(void *private_data, int force_type, int fd)
{
	struct squashfs_private_data *p = private_data;
	int ret;

	ret = fileread(fd, &p->sb, sizeof(p->sb));
	if (ret < 0)
		return ret;

#define squashfs_check_sb(sb, sec, expect) do {					\
	if (le32_to_cpu((sb)->sec) != expect) {					\
		fprintf_if_force_type("Error: %s doesn't match\n", #sec);	\
		return -1;							\
	}									\
} while (0)

#define squashfs_check_sb_start(sb, start1, start2) do {			\
	if (le64_to_cpu((sb)->start1 > le64_to_cpu((sb)->start2))) {		\
		fprintf_if_force_type("Error: %s > %s\n", #start1, #start2);	\
		return -1;							\
	}									\
} while (0)

	squashfs_check_sb(&p->sb, s_magic, SQUASHFS_MAGIC);
	squashfs_check_sb(&p->sb, s_major, SQUASHFS_MAJOR);
	squashfs_check_sb(&p->sb, s_minor, SQUASHFS_MINOR);

	squashfs_check_sb_start(&p->sb, inode_table_start, directory_table_start);
	squashfs_check_sb_start(&p->sb, directory_table_start, fragment_table_start);
	squashfs_check_sb_start(&p->sb, fragment_table_start, lookup_table_start);

#undef squashfs_check_sb
#undef squashfs_check_sb_start

	if (le64_to_cpu(p->sb.bytes_used) > (unsigned)filelength(fd)) {
		fprintf_if_force_type("Error: file is incompleted\n");
		return -1;
	}

	return 0;
}

static int64_t squashfs_total_size(void *private_data, int fd)
{
	struct squashfs_private_data *p = private_data;

	return le64_to_cpu(p->sb.bytes_used);
}

static int squashfs_summary(void *private_data, int fd, char *buf, size_t bufsz)
{
	struct squashfs_private_data *p = private_data;

	snprintf(buf, bufsz, "SIZE: %" PRIu64, squashfs_total_size(p, fd));
	return 0;
}

static int squashfs_init(struct squashfs_private_data *p, int fd)
{
	uint64_t dir_table_end = le64_to_cpu(p->sb.fragment_table_start);
	int ret;

	p->inode_table =
		squashfs_alloc_metablocks(p, fd,
					  le64_to_cpu(p->sb.inode_table_start),
					  /* limit offset */
					  le64_to_cpu(p->sb.directory_table_start),
					  0, /* limit blocks */
					  &p->inode_table_size);
	if (!p->inode_table) {
		squashfs_exit(p);
		return -1;
	}

	ret = squashfs_read_ppoint(p, fd,
				   le64_to_cpu(p->sb.id_table_start),
				   le32_to_cpu(p->sb.no_ids),
				   &p->uids);
	if (ret < 0) {
		fprintf(stderr, "Error: read id tables failed\n");
		squashfs_exit(p);
		return ret;
	}

	ret = squashfs_read_ppoint(p, fd,
				   le64_to_cpu(p->sb.fragment_table_start),
				   le32_to_cpu(p->sb.fragments),
				   &p->fragments);
	if (ret < 0) {
		fprintf(stderr,"Error: read fragment tables failed\n");
		squashfs_exit(p);
		return ret;
	}

	/* squashfs archive layout:
	 * ...
	 * Inode table
	 * Directory table
	 * Fragment table
	 * ...
	 */
	if (p->fragments.arrays[0].global_offset < dir_table_end)
		dir_table_end = p->fragments.arrays[0].global_offset;

	p->dir_table =
		squashfs_alloc_metablocks(p, fd,
					  le64_to_cpu(p->sb.directory_table_start),
					  dir_table_end,
					  0,
					  &p->dir_table_size);
	if (!p->dir_table) {
		squashfs_exit(p);
		return -1;
	}

	return 0;
}

static int squashfs_do_sblock(struct squashfs_private_data *p, int fd,
			      int argc, char **argv)
{
	structure_print(PRINT_LEVEL0, &p->sb, st_squashfs_sblock);
	return 0;
}

static int snprintf_squashfs_inode_type(char *buf, size_t bufsz, int inode_type)
{
	#define case_inode_type(type)					\
		case SQUASHFS_##type##_TYPE:				\
			snprintf(buf, bufsz, "%s ", #type);		\
			break;
	switch (inode_type) {
	case_inode_type(DIR);
	case_inode_type(REG);
	case_inode_type(SYMLINK);
	case_inode_type(BLKDEV);
	case_inode_type(CHRDEV);
	case_inode_type(FIFO);
	case_inode_type(SOCKET);
	case_inode_type(LDIR);
	case_inode_type(LREG);
	case_inode_type(LSYMLINK);
	case_inode_type(LBLKDEV);
	case_inode_type(LCHRDEV);
	case_inode_type(LFIFO);
	case_inode_type(LSOCKET);
	default:
		snprintf(buf, bufsz, "??? (0x%04x)", inode_type);
		return -1;
	}
	#undef case_inode_type

	return 0;
}

static int squashfs_print_base_inode(struct squashfs_ppoint *uids,
				     struct squashfs_base_inode *base)
{
	unsigned uid = le16_to_cpu(base->uid), gid = le16_to_cpu(base->guid);
	struct squashfs_point *id_point = &uids->arrays[0];
	__le32 *id_tables = id_point->buf;
	size_t max_idx = id_point->size - sizeof(*id_tables);
	char inode_type[32];
	int ret;

	ret = snprintf_squashfs_inode_type(inode_type, sizeof(inode_type),
					   le16_to_cpu(base->inode_type));
	if (ret < 0) {
		printf("%s\n", inode_type);
		return ret;
	}

	if (uid > max_idx || gid > max_idx) {
		fprintf(stderr, "id overflow\n");
		return -1;
	}

	printf("%-8s %4o %4d %4d %5d ",
		inode_type,
		le16_to_cpu(base->mode),
		le32_to_cpu(id_tables[uid]),
		le32_to_cpu(id_tables[gid]),
		le32_to_cpu(base->inode_number));

	return 0;
}

static int squashfs_do_inodes(struct squashfs_private_data *squashfs, int fd,
			      int argc, char **argv)
{
	long sz = (long)squashfs->inode_table_size;
	void *p = squashfs->inode_table;

	if (get_verbose_level() > 0)
		hexdump(p, sz, 0);

	printf("%8s %-8s %4s %4s %4s %5s\n",
		"", "type", "mode", "uid", "gid", "inode");

	while (sz > (long)sizeof(struct squashfs_base_inode)) {
		struct squashfs_base_inode *base = p;
		union squashfs_inode *inode = p;
		size_t this_size = 0;
		int ret;

		printf("%08x ", (unsigned int)(p - squashfs->inode_table));

		ret = squashfs_print_base_inode(&squashfs->uids, base);
		if (ret < 0)
			return ret;

		switch (le16_to_cpu(base->inode_type)) {
		case SQUASHFS_REG_TYPE:
			printf("offset=0x%04x block=%u fragment=%u filesize=%u",
				le32_to_cpu(inode->reg.offset),
				le32_to_cpu(inode->reg.start_block),
				le32_to_cpu(inode->reg.fragment),
				le32_to_cpu(inode->reg.file_size));
			this_size += sizeof(inode->reg);
			break;
		case SQUASHFS_DIR_TYPE:
			printf("offset=0x%04x block=%u nlink=%u filesize=%u parent=%u",
				le16_to_cpu(inode->dir.offset),
				le32_to_cpu(inode->dir.start_block),
				le32_to_cpu(inode->dir.nlink),
				le16_to_cpu(inode->dir.file_size),
				le32_to_cpu(inode->dir.parent_inode));
			this_size += sizeof(inode->dir);
			break;
		case SQUASHFS_SYMLINK_TYPE:
			printf("symlink=%.*s",
				le32_to_cpu(inode->symlink.symlink_size),
				inode->symlink.symlink);
			this_size += sizeof(inode->symlink);
			this_size += le32_to_cpu(inode->symlink.symlink_size);
			break;
		case SQUASHFS_LREG_TYPE:
			printf("offset=0x%04x block=%u nlink=%u fragment=%u filesize=%u",
				le32_to_cpu(inode->lreg.offset),
				le32_to_cpu(inode->lreg.start_block),
				le32_to_cpu(inode->lreg.nlink),
				le32_to_cpu(inode->lreg.fragment),
				le32_to_cpu(inode->lreg.file_size));
			this_size += sizeof(inode->lreg);
			break;
		case SQUASHFS_LDIR_TYPE:
			printf("offset=0x%04x i_count=%u parent=%u",
				le16_to_cpu(inode->ldir.offset),
				le16_to_cpu(inode->ldir.i_count),
				le32_to_cpu(inode->ldir.parent_inode));
			this_size += sizeof(inode->ldir);

			for (int i = 0; i < le16_to_cpu(inode->ldir.i_count); i++) {
				struct squashfs_dir_index *index =
						&inode->ldir.index[i];

				printf("\n%38s index=%u block=%u size=%u name=%.*s",
					" ",
					le32_to_cpu(index->index),
					le32_to_cpu(index->start_block),
					le32_to_cpu(index->size),
					le32_to_cpu(index->size) + 1, index->name);

				this_size += le32_to_cpu(index->size) + 1;
				this_size += sizeof(*index);
			}
			break;
		}

		if (this_size == 0) {
			fflush(stdout);
			fprintf(stderr, "Error: unknown inode size\n");
			return -1;
		}

		printf("\n");

		sz -= this_size;
		p += this_size;
	}

	return 0;
}

static int squashfs_do_dirs(struct squashfs_private_data *squashfs,
			   int fd, int argc, char **argv)
{
	const size_t min_size = sizeof(struct squashfs_dir_header)
				+ sizeof(struct squashfs_dir_entry);
	long sz = (long)squashfs->dir_table_size;
	void *p = squashfs->dir_table;

	if (get_verbose_level() > 0)
		hexdump(p, sz, 0);

	printf("%13s %-8s %5s %-6s name\n",
		"", "type", "inode", "off");

	while (sz > (long)(min_size)) {
		struct squashfs_dir_header *h = p;
		/* The counter is stored off-by-one, a value of 0 indicates 1
		 * entry follows. This also makes it impossible to encode a
		 * size of 0, which wouldn't make any sense.
		 */
		uint32_t entry_count = le32_to_cpu(h->count) + 1;
		int ret;

		p += sizeof(*h);
		sz -= sizeof(*h);

		for (uint32_t i = 0;
		     i < entry_count
		     && sz > (long)sizeof(struct squashfs_dir_entry);
		     i++) {
			struct squashfs_dir_entry *entry = p;
			size_t this_size = sizeof(*entry);
			char inode_type[32];

			printf("%08x+%04x ",
				(unsigned int)((void *)h - squashfs->dir_table),
				(unsigned int)(p - (void *)h));

			ret = snprintf_squashfs_inode_type(inode_type,
						   sizeof(inode_type),
						   le16_to_cpu(entry->type));
			if (ret < 0) {
				printf("%-8s\n", inode_type);
				return ret;
			}

			printf("%-8s ", inode_type);
			printf("%-5d ",
				le32_to_cpu(h->inode_number)
				+ le16_to_cpu(entry->inode_number));
			printf("0x%04x ", le16_to_cpu(entry->offset));

			/* size: one less than the size of the entry name */
			printf("%.*s\n", le16_to_cpu(entry->size) + 1, entry->name);
			this_size += le16_to_cpu(entry->size) + 1;

			sz -= this_size;
			p += this_size;
		}
	}

	return 0;
}

static int squashfs_do_fragments(struct squashfs_private_data *squashfs,
				 int fd, int argc, char **argv)
{
	struct squashfs_ppoint *fragments = &squashfs->fragments;
	int verbose = get_verbose_level();

	for (size_t i = 0; i < fragments->array_count; i++) {
		struct squashfs_point *frag = &fragments->arrays[i];
		struct squashfs_fragment_entry *entry = frag->buf;
		size_t sz = frag->size;

		printf("fragment %zu/%zu:\n", i + 1, fragments->array_count);

		if (verbose)
			hexdump(frag->buf, frag->size, 0);

		printf("%8s %8s %4s\n", " ", "start", "size");
		while (sz >= sizeof(*entry)) {
			uint32_t entry_size = le32_to_cpu(entry->size);

			printf("%08x ",
				(unsigned int)((void *)entry - frag->buf));
			printf("%8d ", le32_to_cpu(entry->start_block));
			printf("%4d ",
				SQUASHFS_COMPRESSED_SIZE_BLOCK(entry_size));
			if (SQUASHFS_COMPRESSED_BLOCK(entry_size))
				printf(" (compressed)");
			printf("\n");

			sz -= sizeof(*entry);
			entry++;
		}
	}

	return 0;
}

/*
 * Usage: "hexdump offset size" or "hexdump offset1 offset2 size"
 */
static int squashfs_do_hexdump(struct squashfs_private_data *squashfs,
			       int fd, int argc, char **argv)
{
	unsigned long offset, size, para[3] = { 0 };
	uint8_t *buf;
	int ret;

	if (argc != 3 && argc != 4) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  hexdump offset size\n");
		fprintf(stderr, "  hexdump offset1 offset2 size\n");
		return -1;
	}

	for (int i = 1; i < argc; i++) {
		int err = 0;

		para[i - 1] = strict_strtoul(argv[i], 0, &err, NULL);
		if (err) {
			fprintf(stderr, "Error: bad number %s\n", argv[i]);
			return -1;
		}
	}

	switch (argc) {
	case 3:
		offset = para[0];
		size = para[1];
		break;
	default:
		offset = para[0] + para[1];
		size = para[2];
		break;
	}

	if (size == 0)
		return 0;

	buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "Error: alloc buf failed\n");
		return -1;
	}

	fileseek(fd, offset);
	ret = fileread(fd, buf, size);
	if (!ret)
		hexdump(buf, size, offset);

	free(buf);
	return ret;
}

static int squashfs_nocomp(struct squashfs_super_block *sb)
{
	uint32_t flags = le32_to_cpu(sb->flags);

	return SQUASHFS_UNCOMPRESSED_INODES(flags)
		&& SQUASHFS_UNCOMPRESSED_DATA(flags)
		&& SQUASHFS_UNCOMPRESSED_FRAGMENTS(flags);
}

static int get_filepath_byfd(int fd, char *buf, size_t bufsz)
{
	char proc_self_fd[64];
	ssize_t ret;

	memset(buf, 0, bufsz);
	snprintf(proc_self_fd, sizeof(proc_self_fd), "/proc/self/fd/%d", fd);
	ret = readlink(proc_self_fd, buf, bufsz - 1);
	if (ret < 0 || ret >= (int)bufsz)
		return -1;

	return 0;
}

static int squashfs_list(struct squashfs_private_data *p, int fd)
{
	char filepath[256], shellcmd[2048];
	int ret;

	ret = get_filepath_byfd(fd, filepath, sizeof(filepath));
	if (ret < 0)
		return ret;

	snprintf(shellcmd, sizeof(shellcmd),
		"file=%s\n"
		"unsquashfs -lln -d \"$(basename ${file}).dump\" ${file}\n"
		, filepath
		);
	ret = system(shellcmd);
	if (ret != 0) {
		fprintf(stderr, "Error: run unsquashfs failed\n");
		return -1;
	}

	return ret;
}

static int squashfs_list_main(void *private_data, int fd, int argc, char **argv)
{
	struct squashfs_private_data *p = private_data;

#define assert_init_successful(p, fd) do {					\
	int ret = squashfs_init(p, fd);						\
	if (ret < 0) {								\
		fprintf(stderr, "Error: squashfs init failed\n");		\
		return ret;							\
	}									\
} while (0)

#define assert_no_compression(sb) do {						\
	if (!squashfs_nocomp(sb)) {						\
		fprintf(stderr, "Errorr: squashfs was compressed and "		\
			" we don't support it now\n");				\
		return -1;							\
	}									\
} while (0)

	if (argc >= 1) {
		if (!strcmp(argv[0], "sblock")) {
			return squashfs_do_sblock(private_data, fd, argc, argv);
		} else if (!strcmp(argv[0], "inodes")) {
			assert_no_compression(&p->sb);
			assert_init_successful(p, fd);
			return squashfs_do_inodes(private_data, fd, argc, argv);
		} else if (!strcmp(argv[0], "dirs")) {
			assert_no_compression(&p->sb);
			assert_init_successful(p, fd);
			return squashfs_do_dirs(private_data, fd, argc, argv);
		} else if (!strcmp(argv[0], "fragments")) {
			assert_no_compression(&p->sb);
			assert_init_successful(p, fd);
			return squashfs_do_fragments(private_data, fd, argc, argv);
		} else if (!strcmp(argv[0], "hexdump")) {
			return squashfs_do_hexdump(private_data, fd, argc, argv);
		}
	}

#undef assert_no_compression
#undef assert_init_successful

	return squashfs_list(p, fd);
}

static int squashfs_unpack(void *private_data, int fd, const char *outdir,
			  int argc, char **argv)
{
	char filepath[256], shellcmd[2048];
	int ret;

	ret = get_filepath_byfd(fd, filepath, sizeof(filepath));
	if (ret < 0)
		return ret;

	snprintf(shellcmd, sizeof(shellcmd),
		"outdir=\"%s\"\n"
		"rm -rf \"${outdir}\"\n"
		"unsquashfs -d \"${outdir}\" %s\n"
		, outdir, filepath
		);
	ret = system(shellcmd);
	if (ret != 0) {
		fprintf(stderr, "Error: unsquashfs failed\n");
		return -1;
	}

	return 0;
}

static int squashfs_pack(void *private_data, const char *dir, int fd_outimg,
			 int argc, char **argv)
{
	char filepath[256], shellcmd[2048];
	int ret;

	ret = get_filepath_byfd(fd_outimg, filepath, sizeof(filepath));
	if (ret < 0)
		return ret;

	snprintf(shellcmd, sizeof(shellcmd),
		 "mksquashfs %s %s -noappend", dir, filepath);
	ret = system(shellcmd);
	if (ret != 0) {
		fprintf(stderr, "Error: mksquash failed\n");
		return -1;
	}

	return 0;
}

static struct imgeditor squashfs_editor = {
	.name			= "squashfs",
	.descriptor		= "squashfs image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= sizeof(struct squashfs_super_block),
	.private_data_size	= sizeof(struct squashfs_private_data),
	.detect			= squashfs_detect,
	.total_size		= squashfs_total_size,
	.summary		= squashfs_summary,
	.exit			= squashfs_exit,
	.list			= squashfs_list_main,
	.pack			= squashfs_pack,
	.unpack			= squashfs_unpack,

	.search_magic = {
		.magic		= squashfs_magic,
		.magic_sz	= sizeof(squashfs_magic),
		.magic_offset	= offsetof(struct squashfs_super_block, s_magic),
	}
};
REGISTER_IMGEDITOR(squashfs_editor);
