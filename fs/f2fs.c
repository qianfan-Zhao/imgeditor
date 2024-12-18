/*
 * f2fs
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdlib.h>
#include <string.h>

#include "f2fs_fs.h"
#include "structure.h"
#include "libcrc.h"
#include "string_helper.h"
#include "bitmask.h"

struct f2fs_editor;

#define PRINT_LEVEL0			"%-28s: "
#define PRINT_LEVEL1			"%-30s: "

static void f2fs_sb_print_extension_list(const char *print_name_fmt,
					 const char *name,
					 const void *addr, size_t sz)
{
	const struct f2fs_super_block *sblock =
		container_of(addr, struct f2fs_super_block, extension_list);

	if (sz != sizeof(sblock->extension_list))
		return;

	structure_print_name(print_name_fmt, name);
	for (int i = 0; i < F2FS_MAX_EXTENSION; i++)
		printf("%.*s ", F2FS_EXTENSION_LEN, sblock->extension_list[i]);
	printf("\n");
}

static const struct structure_item st_f2fs_device[] = {
	STRUCTURE_ITEM(struct f2fs_device, path,		structure_item_print_string),
	STRUCTURE_ITEM(struct f2fs_device, total_segments,	structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static void f2fs_sb_print_devs(const char *print_name_fmt, const char *name,
			       const void *addr, size_t sz)
{
	const struct f2fs_device *devs = addr;

	structure_print_name(print_name_fmt, name);

	for (int i = 0; i < MAX_DEVICES; i++) {
		const struct f2fs_device *dev = &devs[i];

		if (dev->path[0] == '\0')
			break;

		structure_print(PRINT_LEVEL1, dev, st_f2fs_device);
	}
	printf("\n");
}

static void f2fs_sb_print_feature(const char *print_name_fmt, const char *name,
				  const void *addr, size_t sz)
{
	#define SB_FEATURE(name) { F2FS_FEATURE_##name, #name }
	static const struct structure_bit_descriptor sb_features[] = {
		SB_FEATURE(ENCRYPT),
		SB_FEATURE(BLKZONED),
		SB_FEATURE(ATOMIC_WRITE),
		SB_FEATURE(EXTRA_ATTR),
		SB_FEATURE(PRJQUOTA),
		SB_FEATURE(INODE_CHKSUM),
		SB_FEATURE(FLEXIBLE_INLINE_XATTR),
		SB_FEATURE(QUOTA_INO),
		SB_FEATURE(INODE_CRTIME),
		SB_FEATURE(LOST_FOUND),
		SB_FEATURE(VERITY),
		SB_FEATURE(SB_CHKSUM),
		SB_FEATURE(CASEFOLD),
		SB_FEATURE(COMPRESSION),
		SB_FEATURE(RO),
		SB_FEATURE(DEVICE_ALIAS),
		{ 0, NULL }
	};
	#undef SB_FEATURE

	return structure_item_print_bit_flags(print_name_fmt, name, addr, sz,
					      sb_features);
}

static const struct structure_item st_f2fs_super_block[] = {
	STRUCTURE_ITEM(struct f2fs_super_block, major_ver,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, minor_ver,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, log_sectorsize,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, log_sectors_per_block,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, log_blocksize,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, log_blocks_per_seg,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segs_per_sec,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, secs_per_zone,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, checksum_offset,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, block_count,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, section_count,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segment_count,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segment_count_ckpt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segment_count_sit,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segment_count_nat,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segment_count_ssa,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segment_count_main,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, segment0_blkaddr,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, cp_blkaddr,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, sit_blkaddr,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, nat_blkaddr,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, ssa_blkaddr,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, main_blkaddr,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, root_ino,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, node_ino,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, meta_ino,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, uuid,				structure_item_print_x8_array),
	STRUCTURE_ITEM(struct f2fs_super_block, volume_name,			structure_item_print_string),
	STRUCTURE_ITEM(struct f2fs_super_block, extension_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, extension_list,			f2fs_sb_print_extension_list),
	STRUCTURE_ITEM(struct f2fs_super_block, cp_payload,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, version,			structure_item_print_string),
	STRUCTURE_ITEM(struct f2fs_super_block, init_version,			structure_item_print_string),
	STRUCTURE_ITEM(struct f2fs_super_block, feature,			f2fs_sb_print_feature),
	STRUCTURE_ITEM(struct f2fs_super_block, encryption_level,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, encrypt_pw_salt,		structure_item_print_x8_array),
	STRUCTURE_ITEM(struct f2fs_super_block, devs,				f2fs_sb_print_devs),
	STRUCTURE_ITEM(struct f2fs_super_block, qf_ino,				structure_item_print_u32_array),
	STRUCTURE_ITEM(struct f2fs_super_block, hot_ext_count,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, s_encoding,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct f2fs_super_block, s_encoding_flags,		structure_item_print_x8_array),
	STRUCTURE_ITEM(struct f2fs_super_block, s_stop_reason,			structure_item_print_x8_array),
	STRUCTURE_ITEM(struct f2fs_super_block, s_errors,			structure_item_print_x8_array),
	STRUCTURE_ITEM(struct f2fs_super_block, crc,				structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

static void f2fs_checkpoint_print_flags(const char *print_name_fmt,
					const char *name,
					const void *addr, size_t sz)
{
	#define CP_FLAG(name) { CP_##name##_FLAG, #name }
	static const struct structure_bit_descriptor sb_features[] = {
		CP_FLAG(RESIZEFS),
		CP_FLAG(DISABLED),
		CP_FLAG(QUOTA_NEED_FSCK),
		CP_FLAG(LARGE_NAT_BITMAP),
		CP_FLAG(NOCRC_RECOVERY),
		CP_FLAG(TRIMMED),
		CP_FLAG(NAT_BITS),
		CP_FLAG(CRC_RECOVERY),
		CP_FLAG(FASTBOOT),
		CP_FLAG(FSCK),
		CP_FLAG(ERROR),
		CP_FLAG(COMPACT_SUM),
		CP_FLAG(ORPHAN_PRESENT),
		CP_FLAG(UMOUNT),
		{ 0, NULL }
	};
	#undef CP_FLAG

	return structure_item_print_bit_flags(print_name_fmt, name, addr, sz,
					      sb_features);
}

static const struct structure_item st_f2fs_checkpoint[] = {
	STRUCTURE_ITEM(struct f2fs_checkpoint, checkpoint_ver,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, user_block_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, valid_block_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, rsvd_segment_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, overprov_segment_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, free_segment_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, cur_node_segno,			structure_item_print_x32_array),
	STRUCTURE_ITEM(struct f2fs_checkpoint, cur_node_blkoff,			structure_item_print_u16_array),
	STRUCTURE_ITEM(struct f2fs_checkpoint, cur_data_segno,			structure_item_print_x32_array),
	STRUCTURE_ITEM(struct f2fs_checkpoint, cur_data_blkoff,			structure_item_print_u16_array),
	STRUCTURE_ITEM(struct f2fs_checkpoint, ckpt_flags,			f2fs_checkpoint_print_flags),
	STRUCTURE_ITEM(struct f2fs_checkpoint, cp_pack_total_block_count,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, cp_pack_start_sum,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, valid_node_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, valid_inode_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, next_free_nid,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, sit_ver_bitmap_bytesize,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, nat_ver_bitmap_bytesize,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, checksum_offset,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, elapsed_time,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_checkpoint, alloc_type,			structure_item_print_x8_array),
	STRUCTURE_ITEM_END(),
};

static void f2fs_inode_print_mode(const char *print_name_fmt, const char *name,
				  const void *p, size_t sz)
{
	uint16_t mode = le16_to_cpu(*(const __le16 *)p);

	structure_print_name(print_name_fmt, name);
	printf("0x%04x (", mode);

	switch (mode & S_IFMT) {
	case S_IFIFO:
		printf("FIFO");
		break;
	case S_IFCHR:
		printf("CHAR");
		break;
	case S_IFDIR:
		printf("Directory");
		break;
	case S_IFBLK:
		printf("Block");
		break;
	case S_IFREG:
		printf("File");
		break;
	case S_IFLNK:
		printf("Symlink");
		break;
	case S_IFSOCK:
		printf("Socket");
		break;
	}

	printf(" %o)", mode & ~S_IFMT);
	printf("\n");
}

static void f2fs_inode_print_inline(const char *print_name_fmt, const char *name,
				     const void *addr, size_t sz)
{
	static const struct structure_bit_descriptor inline_bits[] = {
		{ F2FS_INLINE_XATTR,		"INLINE_XATTR" },
		{ F2FS_INLINE_DATA,		"INLINE_DATA" },
		{ F2FS_INLINE_DENTRY,		"INLINE_DENTRY" },
		{ F2FS_DATA_EXIST,		"DATA_EXIST" },
		{ F2FS_INLINE_DOTS,		"INLINE_DOTS" },
		{ F2FS_EXTRA_ATTR,		"EXTRA_ATTR" },
		{ F2FS_PIN_FILE,		"PIN_FILE" },
		{ F2FS_COMPRESS_RELEASED,	"COMPRESS_RELEASED" },
		{ 0, NULL },
	};

	return structure_item_print_bit_flags(print_name_fmt, name, addr, sz,
					      inline_bits);
}

static const struct structure_item st_f2fs_inode[] = {
	STRUCTURE_ITEM(struct f2fs_inode, i_mode,			f2fs_inode_print_mode),
	STRUCTURE_ITEM(struct f2fs_inode, i_advise,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_inline,			f2fs_inode_print_inline),
	STRUCTURE_ITEM(struct f2fs_inode, i_uid,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_gid,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_links,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_size,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_blocks,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_atime,			structure_item_print_unix_epoch),
	STRUCTURE_ITEM(struct f2fs_inode, i_ctime,			structure_item_print_unix_epoch),
	STRUCTURE_ITEM(struct f2fs_inode, i_mtime,			structure_item_print_unix_epoch),
	STRUCTURE_ITEM(struct f2fs_inode, i_atime_nsec,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_ctime_nsec,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_mtime_nsec,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_generation,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_current_depth,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_xattr_nid,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_flags,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_pino,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_namelen,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_name,			structure_item_print_string),
	STRUCTURE_ITEM(struct f2fs_inode, i_dir_level,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_ext.fofs,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_ext.blk,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_ext.len,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_inode, i_addr,			structure_item_print_hexdump),
	STRUCTURE_ITEM(struct f2fs_inode, i_nid,			structure_item_print_u32_array),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_f2fs_node_footer[] = {
	STRUCTURE_ITEM(struct node_footer, nid,				structure_item_print_unsigned),
	STRUCTURE_ITEM(struct node_footer, ino,				structure_item_print_unsigned),
	STRUCTURE_ITEM(struct node_footer, flag,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct node_footer, cp_ver,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct node_footer, next_blkaddr,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_f2fs_dir_entry[] = {
	STRUCTURE_ITEM(struct f2fs_dir_entry, hash_code,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct f2fs_dir_entry, ino,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_dir_entry, name_len,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct f2fs_dir_entry, file_type,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static void f2fs_sit_entry_print_vblocks(const char *print_name_fmt,
					 const char *name,
					 const void *addr, size_t sz)
{
	static const char *types[] = {
		[CURSEG_COLD_DATA] = "COLD_DATA",
		[CURSEG_WARM_DATA] = "WARM_DATA",
		[CURSEG_HOT_DATA] = "HOT_DATA",
		[CURSEG_COLD_NODE] = "COLD_NODE",
		[CURSEG_WARM_NODE] = "WARM_NODE",
		[CURSEG_HOT_NODE] = "HOT_NODE",
	};
	uint16_t vblocks = le16_to_cpu(*(const __le16 *)addr);
	uint16_t n = vblocks & SIT_VBLOCKS_MASK;
	uint16_t type = (vblocks & ~SIT_VBLOCKS_MASK) >> SIT_VBLOCKS_SHIFT;

	structure_print_name(print_name_fmt, name);
	printf("0x%04x (", vblocks);
	printf("blkcnt=%u", n);
	if (type < NO_CHECK_TYPE)
		printf(" %s", types[type]);
	printf(")\n");
}

static const struct structure_item st_f2fs_sit_entry[] = {
	STRUCTURE_ITEM(struct f2fs_sit_entry, vblocks,			f2fs_sit_entry_print_vblocks),
	STRUCTURE_ITEM(struct f2fs_sit_entry, valid_map,		structure_item_print_hexdump),
	STRUCTURE_ITEM(struct f2fs_sit_entry, mtime,			structure_item_print_unix_epoch),
	STRUCTURE_ITEM_END(),
};

struct f2fs_segment {
	uint32_t			block_size;
	uint32_t			blocks;
	uint8_t				buffer[0];
};

static inline void f2fs_free_segment(struct f2fs_segment *segment)
{
	free(segment);
}

struct f2fs_editor {
	int				fd;

	struct f2fs_super_block		sblock;
	struct f2fs_checkpoint		*checkpoint;
	struct f2fs_segment		*nat;

	uint32_t			sector_size;
	uint32_t			block_size;
	uint32_t			blocks_per_segment;
	uint32_t			segment_size;

	struct libcrc32			crc32_algo;
};

static int f2fs_editor_init(void *private_data)
{
	struct f2fs_editor *f2fs = private_data;

	f2fs->crc32_algo.poly = 0x04C11DB7;
	f2fs->crc32_algo.seed = F2FS_SUPER_MAGIC;
	f2fs->crc32_algo.refin = true;
	f2fs->crc32_algo.refout = true;
	f2fs->crc32_algo.xor_result = 0;

	libcrc32_init(&f2fs->crc32_algo);
	return 0;
}

static void f2fs_editor_exit(void *private_data)
{
	struct f2fs_editor *f2fs = private_data;

	if (f2fs->checkpoint) {
		free(f2fs->checkpoint);
		f2fs->checkpoint = NULL;
	}

	if (f2fs->nat) {
		f2fs_free_segment(f2fs->nat);
		f2fs->nat = NULL;
	}
}

static int f2fs_read_blocks(struct f2fs_editor *p,
			    unsigned long blkno, uint32_t nblks,
			    void *buf, size_t bufsz)
{
	size_t sz = p->block_size * nblks;
	int n;

	if (sz > bufsz)
		sz = bufsz;

	fileseek(p->fd, blkno * p->block_size);
	n = read(p->fd, buf, sz);
	if (n <= 0 || (size_t)n != sz) {
		fprintf(stderr, "Error: read %d blocks from #%lu failed\n",
			nblks, blkno);
		return -1;
	}

	return 0;
}

static void *f2fs_alloc_read_blocks(struct f2fs_editor *f2fs,
				    unsigned long blkno, uint32_t nblks)
{
	size_t sz = f2fs->block_size * nblks;
	void *buf = malloc(sz);

	if (buf) {
		if (f2fs_read_blocks(f2fs, blkno, nblks, buf, sz) < 0) {
			free(buf);
			buf = NULL;
		}
	}

	return buf;
}

static void *f2fs_alloc_read_block(struct f2fs_editor *f2fs,
				   unsigned long blkno)
{
	return f2fs_alloc_read_blocks(f2fs, blkno, 1);
}

static struct f2fs_segment *
	f2fs_alloc_read_segment(struct f2fs_editor *f2fs, uint32_t blkaddr,
				uint32_t segment_count)
{
	size_t total_bytes = segment_count * f2fs->segment_size;
	struct f2fs_segment *seg = malloc(sizeof(*seg) + total_bytes);
	int ret;

	if (!seg) {
		fprintf(stderr, "Error: failed to alloc %u segments\n",
				segment_count);
		return seg;
	}

	seg->block_size = f2fs->block_size;
	seg->blocks = segment_count * f2fs->blocks_per_segment;
	ret = f2fs_read_blocks(f2fs, blkaddr, seg->blocks,
			       seg->buffer, total_bytes);
	if (ret < 0) {
		free(seg);
		return NULL;
	}

	return seg;
}

static int f2fs_get_inode_block(struct f2fs_editor *f2fs, uint32_t ino,
				uint32_t *ret_blkno)
{
	void *p_block = f2fs->nat->buffer;

	for (size_t i = 0; i < f2fs->nat->blocks; i++) {
		struct f2fs_nat_block *nat = p_block;

		for (size_t j = 0; j < NAT_ENTRY_PER_BLOCK; j++) {
			struct f2fs_nat_entry *entry = &nat->entries[j];

			if (le32_to_cpu(entry->ino) == ino) {
				*ret_blkno = le32_to_cpu(entry->block_addr);
				return 0;
			}
		}

		p_block += f2fs->nat->block_size;
	}

	return -1;
}

static struct f2fs_inode *_f2fs_alloc_read_inode(struct f2fs_editor *f2fs,
						uint32_t ino,
						uint32_t *ret_blkno)
{
	struct f2fs_inode *inode = NULL;
	uint32_t blkno;

	if (f2fs_get_inode_block(f2fs, ino, &blkno) < 0) {
		fprintf(stderr, "Error: failed to get inode #%u block\n", ino);
		return inode;
	}

	inode = f2fs_alloc_read_block(f2fs, blkno);
	if (!inode) {
		fprintf(stderr, "Error: failed to alloc inode #%u\n", ino);
		return inode;
	}

	if (ret_blkno)
		*ret_blkno = blkno;

	return inode;
}

static struct f2fs_inode *f2fs_alloc_read_inode(struct f2fs_editor *f2fs,
						uint32_t ino)
{
	return _f2fs_alloc_read_inode(f2fs, ino, NULL);
}

static struct f2fs_nat_entry *
	f2fs_get_nat_entry(struct f2fs_editor *f2fs, uint32_t nid)
{
	struct f2fs_segment *nat = f2fs->nat;
	size_t nat_entry_counts = nat->blocks * NAT_ENTRY_PER_BLOCK;
	struct f2fs_nat_block *nat_block;

	if (nid >= nat_entry_counts)
		return NULL;

	nat_block = (struct f2fs_nat_block *)
		(nat->buffer + (nid / NAT_ENTRY_PER_BLOCK) * f2fs->block_size);

	nid %= NAT_ENTRY_PER_BLOCK;
	return &nat_block->entries[nid];
}

struct f2fs_inode_blocks {
	uint32_t		*blocks;
	size_t			total;

	/* private data */
	size_t			__maxsize;
};

static int f2fs_inode_blocks_push(struct f2fs_editor *p,
				  struct f2fs_inode_blocks *b,
				  uint32_t blkno,
				  struct bitmask *indir_blocks)
{
	if (b->__maxsize == 0) {
		b->total = 0;
		b->__maxsize = DEF_ADDRS_PER_INODE;
		b->blocks = calloc(b->__maxsize, sizeof(*b->blocks));
	} else if (b->total + 1 > b->__maxsize) {
		b->__maxsize *= 2;
		b->blocks = realloc(b->blocks,
				    b->__maxsize * sizeof(*b->blocks));
	}

	if (!b->blocks) {
		fprintf(stderr, "Error: Alloc %zu inode indir blocks failed\n",
			b->__maxsize);
		return -1;
	}

	if (indir_blocks)
		bitmask_set(indir_blocks, blkno);

	b->blocks[b->total] = blkno;
	b->total++;

	return 0;
}

#define f2fs_inode_read_block_define(name, todo, _maxcount)		\
static int								\
f2fs_inode_##name##_blocks_push(struct f2fs_editor *f2fs,		\
				struct f2fs_inode_blocks *b,		\
				uint32_t nid,				\
				struct bitmask *indir_blocks)		\
{									\
	struct f2fs_nat_entry *entry = NULL;				\
	__le32 *blkbuf = NULL; 						\
	uint32_t blkno = 0;						\
	size_t maxcount = _maxcount;					\
	int ret = 0;							\
									\
	entry = f2fs_get_nat_entry(f2fs, nid);				\
	if (!entry) {							\
		fprintf(stderr, "Error: get %s nat entry #%d failed\n",	\
			#name, nid);					\
		return -1;						\
	}								\
									\
	blkno = le32_to_cpu(entry->block_addr);				\
	blkbuf = f2fs_alloc_read_block(f2fs, blkno);			\
	if (!blkbuf) {							\
		fprintf(stderr, "Error: read %s blk #%d failed\n",	\
			#name, blkno);					\
		return -1;						\
	}								\
									\
	if (indir_blocks)						\
		bitmask_set(indir_blocks, blkno);			\
									\
	for (size_t i = 0; i < maxcount; i++) {				\
		uint32_t n = le32_to_cpu(blkbuf[i]);			\
									\
		if (n == 0)						\
			continue;					\
									\
		if (indir_blocks)					\
			bitmask_set(indir_blocks, n);			\
		ret = todo(f2fs, b, n, indir_blocks);			\
		if (ret < 0)						\
			return ret;					\
	}								\
									\
	return ret;							\
}

f2fs_inode_read_block_define(direct, f2fs_inode_blocks_push, DEF_ADDRS_PER_BLOCK);
f2fs_inode_read_block_define(indirect, f2fs_inode_direct_blocks_push, NIDS_PER_BLOCK);
f2fs_inode_read_block_define(double_indirect, f2fs_inode_indirect_blocks_push, NIDS_PER_BLOCK);

static int _f2fs_inode_blocks_read(struct f2fs_editor *f2fs,
				   struct f2fs_inode_blocks *b,
				   struct f2fs_inode *inode,
				   uint32_t ino,
				   struct bitmask *indir_blocks)
{
	int ret = 0;

	for (size_t i = 0; i < DEF_ADDRS_PER_INODE; i++) {
		uint32_t blkno = le32_to_cpu(inode->i_addr[i]);

		/* Yes, the blkno can be zero,
		 * $ imgeditor tests/fs/f2fs/inline_data.f2fs -- inode 3
		 * ...
		 * i_addr                      : 00000000 00 16 00 00 01 16 00 00 03 16 00 00 04 16 00 00 |................|
		 *                             : 00000010 02 16 00 00 05 16 00 00 06 16 00 00 0a 16 00 00 |................|
		 *                             : 00000020 08 16 00 00 0c 16 00 00 07 16 00 00 0b 16 00 00 |................|
		 *                             : 00000030 09 16 00 00 0d 16 00 00 0f 16 00 00 00 00 00 00 |................|
		 *                             : 00000040 14 16 00 00 00 00 00 00 10 16 00 00 00 00 00 00 |................|
		 *                             : 00000050 13 16 00 00 00 00 00 00 0e 16 00 00 00 00 00 00 |................|
		 *                             : 00000060 15 16 00 00 00 00 00 00 11 16 00 00 00 00 00 00 |................|
		 *                             : 00000070 12 16 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
		 *                             : 00000080 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
		 *                             : *
		 *                             : 00000e70
		 * i_nid                       : 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
		 */
		if (blkno == 0)
			continue;

		ret = f2fs_inode_blocks_push(f2fs, b, blkno, indir_blocks);
		if (ret < 0)
			goto done;
	}

	for (int i = 0; i < DEF_NIDS_PER_INODE; i++) {
		uint32_t nid = le32_to_cpu(inode->i_nid[i]);

		if (nid == 0)
			continue;

		switch (i) {
		case 0:
		case 1:
			ret = f2fs_inode_direct_blocks_push(f2fs, b, nid, indir_blocks);
			break;
		case 2:
		case 3:
			ret = f2fs_inode_indirect_blocks_push(f2fs, b, nid, indir_blocks);
			break;
		case 4:
			ret = f2fs_inode_double_indirect_blocks_push(f2fs, b, nid, indir_blocks);
			break;
		}

		if (ret < 0)
			break;
	}

done:
	if (ret < 0) {
		free(b->blocks);
		b->blocks = 0;
		b->total = b->__maxsize = 0;
	}

	return ret;
}

static int f2fs_inode_blocks_read(struct f2fs_editor *p,
				  struct f2fs_inode_blocks *b,
				  struct f2fs_inode *inode,
				  uint32_t ino)
{
	return _f2fs_inode_blocks_read(p, b, inode, ino, NULL);
}

static int f2fs_super_block_check_crc(struct libcrc32 *crc,
				      struct f2fs_super_block *sb,
				      int force_type)
{
	uint32_t value;

	if (!(le32_to_cpu(sb->feature) & F2FS_FEATURE_SB_CHKSUM))
		return 0;

	if (le32_to_cpu(sb->checksum_offset)
		!= offsetof(struct f2fs_super_block, crc)) {
		fprintf_if_force_type("Error: bad checksum_offset\n");
		return -1;
	}

	libcrc32_init(crc);
	libcrc32_update(crc, sb, le32_to_cpu(sb->checksum_offset));
	value = libcrc32_finish(crc);

	if (value != le32_to_cpu(sb->crc)) {
		fprintf_if_force_type("Error: checksum doesn't match. "
				      "%x != %x\n",
				      value, le32_to_cpu(sb->crc));
		return -1;
	}

	return 0;
}

static int f2fs_detect(void *private_data, int force_type, int fd)
{
	struct f2fs_editor *f2fs = private_data;
	struct f2fs_super_block *sb = &f2fs->sblock;
	int ret;

	fileseek(fd, F2FS_SUPER_OFFSET);

	ret = fileread(fd, &f2fs->sblock, sizeof(f2fs->sblock));
	if (ret < 0)
		return ret;

	if (le32_to_cpu(sb->magic) != F2FS_SUPER_MAGIC) {
		fprintf_if_force_type("Error: bad magic\n");
		return -1;
	}

#define assert_lt(sb, name, bits, value) do {				\
	if (le##bits##_to_cpu((sb)->name) >= value) {			\
		fprintf_if_force_type("Error: %s >= %d\n", 		\
				      #name, value);			\
		return -1;						\
	}								\
} while (0)

	assert_lt(sb, major_ver, 16, 2);
	assert_lt(sb, minor_ver, 16, 100);
	assert_lt(sb, log_sectorsize, 32, 20);
	assert_lt(sb, log_sectors_per_block, 32, 20);
	assert_lt(sb, log_blocks_per_seg, 32, 20);
	assert_lt(sb, extension_count, 32, F2FS_MAX_EXTENSION);
#undef assert_lt

	if (le32_to_cpu(sb->log_sectorsize)
	    + le32_to_cpu(sb->log_sectors_per_block) != le32_to_cpu(sb->log_blocksize)) {
		fprintf_if_force_type("Error: sectorsize and blocksize doesn't match\n");
		return -1;
	}

	ret = f2fs_super_block_check_crc(&f2fs->crc32_algo, sb, force_type);
	if (ret < 0)
		return ret;

	f2fs->fd = fd;
	f2fs->sector_size = 1 << le32_to_cpu(sb->log_sectorsize);
	f2fs->block_size = 1 << le32_to_cpu(sb->log_blocksize);
	f2fs->blocks_per_segment = 1 << le32_to_cpu(sb->log_blocks_per_seg);
	f2fs->segment_size = f2fs->block_size * f2fs->blocks_per_segment;

	if (f2fs->block_size != F2FS_BLKSIZE) {
		fprintf_if_force_type("Error: blocksize is not %d\n",
				      F2FS_BLKSIZE);
		return -1;
	}

	f2fs->checkpoint =
		f2fs_alloc_read_block(f2fs, le32_to_cpu(f2fs->sblock.cp_blkaddr));
	if (!f2fs->checkpoint)
		return -1;

	f2fs->nat = f2fs_alloc_read_segment(f2fs,
			le32_to_cpu(f2fs->sblock.nat_blkaddr),
			le32_to_cpu(f2fs->sblock.segment_count_nat));
	if (!f2fs->nat) {
		f2fs_editor_exit(f2fs);
		return -1;
	}

	return 0;
}

static int f2fs_do_sblock(void *private_data, int fd, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;

	structure_print(PRINT_LEVEL0, &f2fs->sblock, st_f2fs_super_block);
	return 0;
}

static int f2fs_do_block_as_sblock(void *blk)
{
	structure_print(PRINT_LEVEL0, blk + F2FS_SUPER_OFFSET,
			st_f2fs_super_block);
	return 0;
}

static int f2fs_do_block_as_inode(struct f2fs_editor *f2fs, void *blk)
{
	struct f2fs_node *node = blk;

	structure_print(PRINT_LEVEL0, &node->i, st_f2fs_inode);
	structure_print(PRINT_LEVEL0, &node->footer, st_f2fs_node_footer);

	return 0;
}

static inline long pointsub(const void *a, const void *b)
{
	/* force convert to long to compat both 64bit and 32bit platform */
	return (long)(a - b);
}

static int f2fs_do_block_as_dentry(struct f2fs_editor *f2fs, void *blk)
{
	struct f2fs_dentry_block *dblk = blk;
	struct bitmask *bitmap = alloc_bitmask(sizeof(dblk->dentry_bitmap) * 8);
	int ret = 0;

	if (!bitmap) {
		fprintf(stderr, "Error: alloc bitmask failed\n");
		return -1;
	}

	bitmask_memcpy_lsbfirst(bitmap, dblk->dentry_bitmap,
				sizeof(dblk->dentry_bitmap));

	printf("%-8s %-8s %-4s %-6s %s\n",
		"hash", "ino", "len", "type", "name");

	bitmask_foreach(i, bitmap) {
		static const char *filetypes[F2FS_FT_MAX] = {
			[F2FS_FT_REG_FILE] = "FILE",
			[F2FS_FT_DIR] = "DIR",
			[F2FS_FT_SYMLINK] = "LINK",
			[F2FS_FT_CHRDEV] = "CHR",
			[F2FS_FT_BLKDEV] = "BLK",
			[F2FS_FT_FIFO] = "FIFO",
			[F2FS_FT_SOCK] = "SOCK",
		};

		struct f2fs_dir_entry *dentry;

		if (i >= NR_DENTRY_IN_BLOCK)
			break;

		dentry = &dblk->dentry[i];

		/* it's ok if the filetype is 0 since the previous
		 * filename length is larger than F2FS_SLOT_LEN.
		 * it will use this entry's filename slot and mark
		 * this entry as used.
		 */
		if (dentry->file_type == 0)
			continue;

		if (dentry->file_type >= F2FS_FT_MAX
		    || le16_to_cpu(dentry->name_len) > F2FS_NAME_LEN) {
			fprintf(stderr, "Error: bad dentry at 0x04%lx\n",
				pointsub(dentry, blk));
			structure_print(PRINT_LEVEL0, dentry, st_f2fs_dir_entry);
			ret = -1;
			break;
		}

		printf("%08x %-8d %-4d %-6s ",
			le32_to_cpu(dentry->hash_code),
			le32_to_cpu(dentry->ino),
			le16_to_cpu(dentry->name_len),
			filetypes[dentry->file_type]);

		printf("%.*s\n", le16_to_cpu(dentry->name_len), dblk->filename[i]);
	}

	bitmask_free(bitmap);
	return ret;
}

/* SIT: segment information table */
static int f2fs_do_block_as_sit_block(struct f2fs_editor *f2fs, void *blk)
{
	struct f2fs_sit_block *sit = blk;

	for (size_t i = 0; i < SIT_ENTRY_PER_BLOCK; i++) {
		struct f2fs_sit_entry *entry = &sit->entries[i];
		uint16_t count = GET_SIT_VBLOCKS(entry);

		if (count == 0)
			continue;

		printf("main segment %zu\n", i);
		structure_print(PRINT_LEVEL0, entry, st_f2fs_sit_entry);
		printf("\n");
	}

	return 0;
}

/* NAT: node address table */
static int f2fs_do_block_as_nat_block(struct f2fs_editor *f2fs, void *blk)
{
	struct f2fs_nat_block *nat = blk;

	printf("%-8s %-8s %-8s\n", "version", "ino", "block");
	for (size_t i = 0; i < NAT_ENTRY_PER_BLOCK; i++) {
		struct f2fs_nat_entry *entry = &nat->entries[i];

		if (le32_to_cpu(entry->ino) == 0)
			continue;

		printf("%-8u %-8u %-8u\n",
			entry->version,
			le32_to_cpu(entry->ino),
			le32_to_cpu(entry->block_addr));
	}

	return 0;
}

static int f2fs_do_list_nat(void *private_data, int fd, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;
	void *p_block = f2fs->nat->buffer;

	printf("%-8s %-8s %-8s\n", "version", "ino", "block");
	for (size_t i = 0; i < f2fs->nat->blocks; i++) {
		struct f2fs_nat_block *nat = p_block;

		for (size_t j = 0; j < NAT_ENTRY_PER_BLOCK; j++) {
			struct f2fs_nat_entry *entry = &nat->entries[j];

			if (le32_to_cpu(entry->ino) == 0)
				continue;

			printf("%-8u %-8u %-8u\n",
				entry->version,
				le32_to_cpu(entry->ino),
				le32_to_cpu(entry->block_addr));
		}

		p_block += f2fs->nat->block_size;
	}

	return 0;
}

static int f2fs_do_nat(void *private_data, int fd, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;
	struct f2fs_nat_entry *entry = NULL;
	int nid = -1;

	if (argc == 1)
		return f2fs_do_list_nat(private_data, fd, argc, argv);

	nid = (int)strtol(argv[1], NULL, 0);
	if (nid < 0) {
		fprintf(stderr, "Error: bad nid %d\n", nid);
		return -1;
	}

	entry = f2fs_get_nat_entry(f2fs, nid);
	if (!entry) {
		fprintf(stderr, "Error: get nat entry #%d failed\n", nid);
		fprintf(stderr, "filesystem has %d segments for nat (%d blocks)"
				", total %zu nat entrys\n",
				le32_to_cpu(f2fs->sblock.segment_count_nat),
				f2fs->nat->blocks,
				f2fs->nat->blocks * NAT_ENTRY_PER_BLOCK);
		return -1;
	}

	printf("%-8s %-8s %-8s\n", "version", "ino", "block");
	printf("%-8u %-8u %-8u\n",
		entry->version,
		le32_to_cpu(entry->ino),
		le32_to_cpu(entry->block_addr));

	return 0;
}

/* show block raw data */
static int f2fs_do_block(void *private_data, int fd, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;
	uint32_t main_blkaddr = le32_to_cpu(f2fs->sblock.main_blkaddr);
	const char *asas = NULL;
	uint32_t base_blkno = 0;
	int ret = 0, blkno = -1;
	uint8_t *blk;

	if (!strcmp(argv[0], "hot_node_block")) {
		base_blkno = main_blkaddr +
			f2fs->blocks_per_segment *
			le32_to_cpu(f2fs->checkpoint->cur_node_segno[0]);
	} else if (!strcmp(argv[0], "warm_node_block")) {
		base_blkno = main_blkaddr +
			f2fs->blocks_per_segment *
			le32_to_cpu(f2fs->checkpoint->cur_node_segno[1]);
	} else if (!strcmp(argv[0], "cold_node_block")) {
		base_blkno = main_blkaddr +
			f2fs->blocks_per_segment *
			le32_to_cpu(f2fs->checkpoint->cur_node_segno[2]);
	} else if (!strcmp(argv[0], "hot_data_block")) {
		base_blkno = main_blkaddr +
			f2fs->blocks_per_segment *
			le32_to_cpu(f2fs->checkpoint->cur_data_segno[0]);
	} else if (!strcmp(argv[0], "warm_data_block")) {
		base_blkno = main_blkaddr +
			f2fs->blocks_per_segment *
			le32_to_cpu(f2fs->checkpoint->cur_data_segno[1]);
	} else if (!strcmp(argv[0], "cold_data_block")) {
		base_blkno = main_blkaddr +
			f2fs->blocks_per_segment *
			le32_to_cpu(f2fs->checkpoint->cur_data_segno[2]);
	}

	/* block #blkno as inode */
	if (argc > 1)
		blkno = (int)strtol(argv[1], NULL, 0);

	if (argc >= 3) {
		if (strcmp(argv[2], "as") || argc < 4)
			goto usage;
		asas = argv[3];
	}

	if (blkno < 0) {
	usage:
		fprintf(stderr, "Usage: f2fs block #blkno ");
		fprintf(stderr, "[ as sblock|inode|direct|indirect ]\n");
		return -1;
	}

	if (strcmp(argv[0], "block") != 0) {
		printf("%s block #%d on #%d\n",
			argv[0], blkno, blkno + base_blkno);
	}

	blkno += base_blkno;
	blk = f2fs_alloc_read_block(f2fs, blkno);
	if (!blk)
		return -1;

	if (!asas)
		hexdump(blk, f2fs->block_size, blkno * f2fs->block_size);
	else if (!strcmp(asas, "sblock"))
		ret = f2fs_do_block_as_sblock(blk);
	else if (!strcmp(asas, "inode"))
		ret = f2fs_do_block_as_inode(f2fs, blk);
	else if (!strcmp(asas, "dentry"))
		ret = f2fs_do_block_as_dentry(f2fs, blk);
	else if (!strcmp(asas, "sit"))
		ret = f2fs_do_block_as_sit_block(f2fs, blk);
	else if (!strcmp(asas, "nat"))
		ret = f2fs_do_block_as_nat_block(f2fs, blk);
	else
		ret = -1;

	free(blk);
	return ret;
}

static int f2fs_do_checkpoint(void *private_data, int fd, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;

	structure_print(PRINT_LEVEL0, f2fs->checkpoint, st_f2fs_checkpoint);
	return 0;
}

static int f2fs_do_inode(void *private_data, int fd, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;
	uint32_t blkno;
	struct f2fs_node *node;
	int ino = -1;

	if (argc > 1)
		ino = (int)strtol(argv[1], NULL, 0); /* auto detect dec or hex */

	if (ino < 0) {
		fprintf(stderr, "Usage: f2fs inode #ino\n");
		return -1;
	}

	node = (struct f2fs_node *)_f2fs_alloc_read_inode(f2fs, ino, &blkno);
	if (!node)
		return -1;

	printf("inode #%d location on blk #%u\n", ino, blkno);
	structure_print(PRINT_LEVEL0, &node->i, st_f2fs_inode);
	structure_print(PRINT_LEVEL0, &node->footer, st_f2fs_node_footer);
	free(node);

	return 0;
}

static bool f2fs_inode_has_nid(struct f2fs_inode *inode)
{
	for (size_t i = 0; i < DEF_NIDS_PER_INODE; i++) {
		if (le32_to_cpu(inode->i_nid[i]) != 0)
			return true;
	}

	return false;
}

typedef int (*foreach_dirent_todo_t)(struct f2fs_editor *f2fs,
				     const char *path, int depth,
				     struct f2fs_dir_entry *dentry,
				     int *break_next,
				     void *foreach_data);

static int _foreach_dirent(struct f2fs_editor *f2fs, uint32_t ino,
			   struct f2fs_inode *inode,
			   struct f2fs_inode_blocks *data_blocks,
			   const char *path, int depth,
			   foreach_dirent_todo_t todo,
			   void *foreach_data);

static int foreach_dirent(struct f2fs_editor *f2fs, uint32_t ino,
			  const char *path, int depth,
			  foreach_dirent_todo_t todo,
			  void *foreach_data)
{
	struct f2fs_inode *inode = f2fs_alloc_read_inode(f2fs, ino);
	struct f2fs_inode_blocks data_blocks = { .blocks = NULL };
	int ret = -1;

	if (!inode)
		return ret;

	if ((le16_to_cpu(inode->i_mode) & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "Error: inode #%d is not a directory\n", ino);
		goto done;
	} else if (le64_to_cpu(inode->i_size) == 0) {
		fprintf(stderr, "Error: inode #%d is empty\n", ino);
		goto done;
	} else if (f2fs_inode_has_nid(inode)) {
		fprintf(stderr, "Error: inode #%d has NID\n", ino);
		goto done;
	}

	ret = f2fs_inode_blocks_read(f2fs, &data_blocks, inode, ino);
	if (ret < 0) {
		fprintf(stderr, "Error: failed to read inode data blocks #%u\n",
			ino);
		goto done;
	}

	ret = _foreach_dirent(f2fs, ino, inode, &data_blocks,
			      path, depth,
			      todo, foreach_data);

done:
	free(data_blocks.blocks);
	free(inode);

	return ret;
}

static int _foreach_dirent(struct f2fs_editor *f2fs, uint32_t ino,
			   struct f2fs_inode *inode,
			   struct f2fs_inode_blocks *data_blocks,
			   const char *path, int depth,
			   foreach_dirent_todo_t todo,
			   void *foreach_data)
{
	struct f2fs_dentry_block *dblk = malloc(f2fs->block_size);
	char child_dir[4096] = { 0 };
	struct bitmask *bitmap = NULL;
	int break_next = 0;
	int ret = -1;

	if (!dblk) {
		fprintf(stderr, "Error: alloc block buffer failed\n");
		goto done;
	}

	bitmap = alloc_bitmask(sizeof(dblk->dentry_bitmap) * 8);
	if (!bitmap) {
		fprintf(stderr, "Error: alloc bitmask failed\n");
		goto done;
	}

	for (size_t i = 0; i < data_blocks->total; i++) {
		ret = f2fs_read_blocks(f2fs, data_blocks->blocks[i], 1,
					dblk, f2fs->block_size);

		if (ret < 0) {
			fprintf(stderr, "Error: failed to read dentry block #%u"
				" of inode #%u\n",
				data_blocks->blocks[i], ino);
			goto done;
		}

		bitmask_memcpy_lsbfirst(bitmap, dblk->dentry_bitmap,
					sizeof(dblk->dentry_bitmap));

		bitmask_foreach(i, bitmap) {
			struct f2fs_dir_entry *dentry;

			if (i >= NR_DENTRY_IN_BLOCK)
				break;

			dentry = &dblk->dentry[i];

			/* . or .. has zero hash value */
			if (le32_to_cpu(dentry->hash_code) == 0)
				continue;

			/* it's ok if the filetype is 0 since the previous
			* filename length is larger than F2FS_SLOT_LEN.
			* it will use this entry's filename slot and mark
			* this entry as used.
			*/
			if (dentry->file_type == 0)
				continue;

			if (dentry->file_type >= F2FS_FT_MAX
			|| le16_to_cpu(dentry->name_len) > F2FS_NAME_LEN) {
				fprintf(stderr, "Error: bad dentry at 0x04%lx\n",
					pointsub(dentry, dblk));
				structure_print(PRINT_LEVEL0, dentry, st_f2fs_dir_entry);
				ret = -1;
				goto done;
			}

			snprintf(child_dir, sizeof(child_dir),
				"%s/%.*s",
				 path,
				 le16_to_cpu(dentry->name_len),
				 dblk->filename[i]);

			ret = todo(f2fs, child_dir, depth, dentry,
				   &break_next, foreach_data);
			if (ret < 0 || break_next)
				goto done;

			if (le16_to_cpu(dentry->file_type) == F2FS_FT_DIR) {
				ret = foreach_dirent(f2fs, le32_to_cpu(dentry->ino),
						     child_dir, depth + 1,
						     todo, foreach_data);
				if (ret < 0)
					goto done;
			}
		}
	}

done:
	if (dblk)
		free(dblk);
	if (bitmap)
		bitmask_free(bitmap);

	return ret;
}

static int print_file(struct f2fs_editor *f2fs, uint32_t ino, int indent)
{
	struct f2fs_inode *inode = f2fs_alloc_read_inode(f2fs, ino);

	if (!inode)
		return -1;

	switch (le16_to_cpu(inode->i_mode) & S_IFMT) {
	case S_IFREG:
		printf("%" PRIu64 " bytes\n", le64_to_cpu(inode->i_size));
		break;
	case S_IFLNK:
		if (!(inode->i_inline & F2FS_DATA_EXIST)) {
			printf("Error: symlink #%u doesn't has inline data\n",
				ino);
			return -1;
		}

		/* i_addr[0] is reserved, the symlink data is from i_addr[1] */
		printf("-> %.*s\n", (int)le64_to_cpu(inode->i_size),
			(const char *)&inode->i_addr[1]);
		break;
	}

	free(inode);
	return 0;
}

static const char *const_basename(const char *path)
{
	const char *p = strrchr(path, '/');

	if (!p)
		return path;

	return p + 1;
}

static int print_dirent_callback(struct f2fs_editor *f2fs,
				 const char *path, int depth,
				 struct f2fs_dir_entry *dentry,
				 int *break_next,
				 void *foreach_data)
{
	static const char *filetypes[F2FS_FT_MAX] = {
		[F2FS_FT_REG_FILE] = "File",
		[F2FS_FT_DIR] = "Directory",
		[F2FS_FT_SYMLINK] = "Link",
		[F2FS_FT_CHRDEV] = "Char",
		[F2FS_FT_BLKDEV] = "Block",
		[F2FS_FT_FIFO] = "FIFO",
		[F2FS_FT_SOCK] = "Socket",
	};
	int ret = 0;

	for (int i = 0; i < depth; i++)
		printf("|   ");

	printf("|-- #%08u: %-10s %.*s ",
		le32_to_cpu(dentry->ino),
		filetypes[dentry->file_type],
		le32_to_cpu(dentry->name_len),
		const_basename(path));

	switch (dentry->file_type) {
	case F2FS_FT_REG_FILE:
	case F2FS_FT_SYMLINK:
		ret = print_file(f2fs, le32_to_cpu(dentry->ino), depth);
		break;
	default:
		printf("\n");
		break;
	}

	return ret;
}

static int unpack_symlink(struct f2fs_editor *f2fs, uint32_t ino,
			  const char *filename)
{
	struct f2fs_inode *inode = f2fs_alloc_read_inode(f2fs, ino);
	uint64_t total_size;
	int ret = -1;

	if (!inode)
		return -1;

	total_size = le64_to_cpu(inode->i_size);
	if (total_size == 0 || !(inode->i_inline & F2FS_DATA_EXIST)) {
		fprintf(stderr, "Error: symlink #%u has no inline data\n",
			ino);
		goto done;
	} else {
		void *data_start = &inode->i_addr[1];
		size_t maxsz = sizeof(inode->i_addr) - sizeof(inode->i_addr[0]);

		if (total_size > maxsz) {
			fprintf(stderr, "Error: inode #%u has %" PRIu64
				" bytes inline data, but only %zu bytes can be read\n",
				ino, total_size, maxsz);
			ret = -1;
			goto done;
		}

		unlink(filename);
		ret = symlink(data_start, filename);
	}

done:
	free(inode);
	return ret;
}

static int unpack_reg_file(struct f2fs_editor *f2fs, uint32_t ino,
			   const char *filename)
{
	struct f2fs_inode *inode = f2fs_alloc_read_inode(f2fs, ino);
	struct f2fs_inode_blocks data_blocks = { .blocks = NULL };
	uint64_t total_size;
	void *blkbuf = NULL;
	int fd = -1, ret = -1;

	if (!inode)
		return -1;

	fd = fileopen(filename, O_CREAT | O_WRONLY | O_TRUNC,
		      le16_to_cpu(inode->i_mode) & ~ S_IFMT);
	if (fd < 0) {
		ret = fd;
		goto done;
	}

	total_size = le64_to_cpu(inode->i_size);
	if (total_size == 0) {
		ret = 0;
		goto done;
	}

	if (inode->i_inline & F2FS_DATA_EXIST) {
		void *data_start = &inode->i_addr[1];
		size_t maxsz = sizeof(inode->i_addr) - sizeof(inode->i_addr[0]);

		if (total_size > maxsz) {
			fprintf(stderr, "Error: inode #%u has %" PRIu64
				" bytes inline data, but only %zu bytes can be read\n",
				ino, total_size, maxsz);
			ret = -1;
			goto done;
		}

		write(fd, data_start, total_size);
		ret = 0;

		goto done;
	}

	blkbuf = malloc(f2fs->block_size);
	if (!blkbuf)
		goto done;

	ret = f2fs_inode_blocks_read(f2fs, &data_blocks, inode, ino);
	if (ret < 0)
		goto done;

	for (size_t i = 0; i < data_blocks.total && total_size > 0; i++) {
		uint32_t blkno = data_blocks.blocks[i];
		size_t chunk_sz = f2fs->block_size;

		if (blkno == 0)
			continue;

		if (total_size < chunk_sz)
			chunk_sz = total_size;

		ret = f2fs_read_blocks(f2fs, blkno, 1, blkbuf, f2fs->block_size);
		if (ret < 0)
			goto done;

		write(fd, blkbuf, chunk_sz);
		total_size -= chunk_sz;
	}

done:
	if (!(fd < 0))
		close(fd);
	free(blkbuf);
	free(data_blocks.blocks);
	free(inode);

	return ret;
}

static int unpack_dir(struct f2fs_editor *f2fs, uint32_t ino, const char *filename)
{
	struct f2fs_inode *inode = f2fs_alloc_read_inode(f2fs, ino);

	if (!inode) {
		fprintf(stderr, "Error: read inode #%u failed\n", ino);
		return -1;
	}

	mkdir(filename, le16_to_cpu(inode->i_mode) & ~ S_IFMT);
	free(inode);

	return 0;
}

static int unpack_dirent_callback(struct f2fs_editor *f2fs,
				  const char *path, int depth,
				  struct f2fs_dir_entry *dentry,
				  int *break_next,
				  void *foreach_data)
{
	int ret = -1;

	switch (dentry->file_type) {
	case F2FS_FT_REG_FILE:
		ret = unpack_reg_file(f2fs, le32_to_cpu(dentry->ino), path);
		break;
	case F2FS_FT_SYMLINK:
		ret = unpack_symlink(f2fs, le32_to_cpu(dentry->ino), path);
		break;
	case F2FS_FT_DIR:
		ret = unpack_dir(f2fs, le32_to_cpu(dentry->ino), path);
		break;
	default:
		fprintf(stderr, "Error: unpack filetype %d not supported\n",
			dentry->file_type);
		break;
	}

	return ret;
}

static int f2fs_default_list(void *private_data, int fd, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;

	return foreach_dirent(f2fs, le32_to_cpu(f2fs->sblock.root_ino),
			      "", 0,
			      print_dirent_callback, NULL);
}

static int f2fs_list_main(void *private_data, int fd, int argc, char **argv)
{
	if (argc >= 1) {
		static const char *block_cmds[] = {
			"block",
			"hot_node_block",
			"warm_node_block",
			"cold_node_block",
			"hot_data_block",
			"warm_data_block",
			"cold_data_block",
			NULL,
		};

		for (size_t i = 0; block_cmds[i]; i++) {
			if (!strcmp(block_cmds[i], argv[0]))
				return f2fs_do_block(private_data, fd, argc, argv);
		}

		if (!strcmp(argv[0], "sblock"))
			return f2fs_do_sblock(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "checkpoint"))
			return f2fs_do_checkpoint(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "nat"))
			return f2fs_do_nat(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "inode"))
			return f2fs_do_inode(private_data, fd, argc, argv);

		fprintf(stderr, "Error: unknown command %s\n", argv[0]);
		return -1;
	}

	return f2fs_default_list(private_data, fd, argc, argv);
}

static int f2fs_unpack(void *private_data, int fd, const char *dirout, int argc, char **argv)
{
	struct f2fs_editor *f2fs = private_data;

	return foreach_dirent(f2fs, le32_to_cpu(f2fs->sblock.root_ino),
			      dirout, 0,
			      unpack_dirent_callback, NULL);
}

static struct imgeditor f2fs_editor = {
	.name			= "f2fs",
	.descriptor		= "f2fs image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= SIZE_MB(1),
	.private_data_size	= sizeof(struct f2fs_editor),
	.init			= f2fs_editor_init,
	.exit			= f2fs_editor_exit,
	.detect			= f2fs_detect,
	.list			= f2fs_list_main,
	.unpack			= f2fs_unpack,
};
REGISTER_IMGEDITOR(f2fs_editor);
