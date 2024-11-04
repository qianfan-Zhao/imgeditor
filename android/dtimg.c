/*
 * for android mkdtimg
 *
 * qianfan Zhao <qianfanguijin@163.com
 */

#include <stdlib.h>
#include "imgeditor.h"
#include "structure.h"

#define PRINT_LEVEL0					"%-30s: "
#define PRINT_LEVEL1					"    %-26s: "
#define PRINT_LEVEL3					"        %-22s: "

#define DT_TABLE_MAGIC 0xd7b7ab1e
#define DT_TABLE_DEFAULT_PAGE_SIZE 2048
#define DT_TABLE_DEFAULT_VERSION 0

static const uint8_t dt_table_magic[4] = {
	(DT_TABLE_MAGIC >> 24) & 0xff,
	(DT_TABLE_MAGIC >> 16) & 0xff,
	(DT_TABLE_MAGIC >>  8) & 0xff,
	(DT_TABLE_MAGIC >>  0) & 0xff,
};

struct dt_table_header {
	__be32 magic;			/* DT_TABLE_MAGIC */
	__be32 total_size;		/* includes dt_table_header + all dt_table_entry
					   and all dtb/dtbo */
	__be32 header_size;		/* sizeof(dt_table_header) */

	__be32 dt_entry_size;		/* sizeof(dt_table_entry) */
	__be32 dt_entry_count;		/* number of dt_table_entry */
	__be32 dt_entries_offset; 	/* offset to the first dt_table_entry
					   from head of dt_table_header.
					   The value will be equal to header_size if
					   no padding is appended */

	__be32 page_size;		/* flash page size we assume */
	__be32 version;			/* DTBO image version, the current version is 0.
					   The version will be incremented when the dt_table_header
					   struct is updated. */
};

static struct structure_item st_dt_table_header[] = {
	STRUCTURE_ITEM(struct dt_table_header, magic,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct dt_table_header, total_size,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_header, header_size,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_header, dt_entry_size,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_header, dt_entry_count,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_header, dt_entries_offset,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_header, page_size,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_header, version,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM_END(),
};

enum dt_compression_info {
	NO_COMPRESSION,
	ZLIB_COMPRESSION,
	GZIP_COMPRESSION
};

struct dt_table_entry {
	__be32 dt_size;
	__be32 dt_offset;		/* offset from head of dt_table_header */

	__be32 id;			/* optional, must be zero if unused */
	__be32 rev;			/* optional, must be zero if unused */
	__be32 custom[4];		/* optional, must be zero if unused */
};

static struct structure_item st_dt_table_entry[] = {
	STRUCTURE_ITEM(struct dt_table_entry, dt_size,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_entry, dt_offset,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct dt_table_entry, id,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct dt_table_entry, rev,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct dt_table_entry, custom,			structure_item_print_be_x32_array),
	STRUCTURE_ITEM_END(),
};

struct dt_table_entry_v1 {
	__be32 dt_size;
	__be32 dt_offset;		/* offset from head of dt_table_header */

	__be32 id;			/* optional, must be zero if unused */
	__be32 rev;			/* optional, must be zero if unused */
	__be32 flags;			/* For version 1 of dt_table_header, the 4 least significant bits
					   of 'flags' will be used indicate the compression
					   format of the DT entry as per the enum 'dt_compression_info' */
	__be32 custom[3];		/* optional, must be zero if unused */
};

struct dtimg_private_data {
	struct dt_table_header		header;

	/* set entries to 'void *' to support dt_table_entry_v1 in the futher */
	void				*entries;
};

#define dtimg_check_section(p, name, expect)					\
	if (be32_to_cpu((p)->header.name) != expect) {				\
		fprintf_if_force_type("%s != 0x%08x\n", 			\
			#name, (unsigned int)expect);				\
		return -1;							\
	}

static int dtimg_detect(void *private_data, int force_type, int fd)
{
	struct dtimg_private_data *p = private_data;
	int ret;

	ret = fileread(fd, &p->header, sizeof(p->header));
	if (ret < 0)
		return ret;

	dtimg_check_section(p, magic, DT_TABLE_MAGIC);
	dtimg_check_section(p, header_size, sizeof(struct dt_table_header));
	dtimg_check_section(p, dt_entry_size, sizeof(struct dt_table_entry));
	dtimg_check_section(p, page_size, DT_TABLE_DEFAULT_PAGE_SIZE);

	/* we not support V1 now */
	dtimg_check_section(p, version, DT_TABLE_DEFAULT_VERSION);

	if (be32_to_cpu(p->header.dt_entries_offset) < sizeof(struct dt_table_header)) {
		fprintf_if_force_type("dt_entries_offset overflow dt_table_header\n");
		return -1;
	}

	p->entries = calloc(be32_to_cpu(p->header.dt_entry_count),
			    be32_to_cpu(p->header.dt_entry_size));
	if (!p->entries) {
		fprintf(stderr, "alloc entries failed\n");
		return -1;
	}

	fileseek(fd, be32_to_cpu(p->header.dt_entries_offset));
	ret = fileread(fd, p->entries,
			be32_to_cpu(p->header.dt_entry_count) *
			be32_to_cpu(p->header.dt_entry_size));
	if (ret < 0) {
		fprintf(stderr, "read entries failed\n");
		free(p->entries);
		return ret;
	}

	return 0;
}

static int64_t dtimg_total_size(void *private_data, int fd)
{
	struct dtimg_private_data *p = private_data;

	return be32_to_cpu(p->header.total_size);
}

static int dtimg_summary(void *private_data, int fd, char *buf, size_t bufsz)
{
	struct dtimg_private_data *p = private_data;
	uint32_t c32 = 0;

	c32 = crc32(c32, (void *)&p->header, sizeof(p->header));
	c32 = crc32(c32, p->entries,
		be32_to_cpu(p->header.dt_entry_size)
		* be32_to_cpu(p->header.dt_entry_count));

	snprintf(buf, bufsz, "CRC32: 0x%08x contains %d fdt",
		c32,
		be32_to_cpu(p->header.dt_entry_count));
	return 0;
}

static int dtimg_list(void *private_data, int fd, int argc, char **argv)
{
	struct dtimg_private_data *p = private_data;
	struct dt_table_entry *entry = p->entries;

	if (get_verbose_level() > 0) {
		structure_print(PRINT_LEVEL0, &p->header, st_dt_table_header);

		for (unsigned i = 0; i < be32_to_cpu(p->header.dt_entry_count); i++) {
			printf("\n");
			printf("DT entry %d:\n", i);
			structure_print(PRINT_LEVEL1, entry, st_dt_table_entry);
			entry++;
		}

		return 0;
	}

	printf("%-5s %-10s %-10s %-40s  %s\n",
		"idx", "id", "rev", "custom", "size");

	for (unsigned i = 0; i < be32_to_cpu(p->header.dt_entry_count); i++) {
		printf("[%02d]: 0x%08x 0x%08x 0x%08x 0x%08x %08x 0x%08x %d\n",
			i,
			be32_to_cpu(entry->id),
			be32_to_cpu(entry->rev),
			be32_to_cpu(entry->custom[0]),
			be32_to_cpu(entry->custom[1]),
			be32_to_cpu(entry->custom[2]),
			be32_to_cpu(entry->custom[3]),
			be32_to_cpu(entry->dt_size));
		entry++;
	}

	return 0;
}

static uint32_t dtimg_get_default_custom(struct dtimg_private_data *p,
					 int which_custom)
{
	struct dt_table_entry *entries = p->entries;
	uint32_t entry_count = be32_to_cpu(p->header.dt_entry_count);
	uint32_t max_count = 0, max, *custom;

	if (which_custom < 0 || which_custom >= 4)
		return 0;

	/* Return the number with the most times */
	custom = calloc(entry_count, sizeof(*custom));
	if (!custom)
		return 0;

	for (unsigned i = 0; i < entry_count; i++) {
		struct dt_table_entry *entry = &entries[i];

		custom[which_custom] = be32_to_cpu(entry->custom[which_custom]);

		/* no default custom value? */
		if (custom[which_custom] == 0) {
			free(custom);
			return 0;
		}
	}

	max = custom[0];
	for (unsigned i = 0; i < entry_count; i++) {
		uint32_t count = 1;

		for (unsigned j = i + 1; j < count; j++) {
			if (custom[i] == custom[j]) {
				++count;

				if (count > max_count)
					max = custom[i];
			}
		}
	}

	free(custom);
	return max;
}

#define dtimg_unpack_entry_section(fp, e, sec, name, def) do {			\
	if (be32_to_cpu(e->sec) != def) {					\
		fprintf(fp, "  %s=0x%x\n", name, be32_to_cpu(e->sec));		\
	}									\
} while (0)

static int dtimg_unpack(void *private_data, int fd, const char *outdir,
			int argc, char **argv)
{
	struct dtimg_private_data *p = private_data;
	struct dt_table_entry *entry = p->entries;
	char filename[1024];
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/dtboimg.cfg", outdir);
	fp = fopen(filename, "w+");
	if (!fp) {
		fprintf(stderr, "Error: create %s failed\n", filename);
		return -1;
	}

	fprintf(fp, "# global options\n");
	for (int custom_idx = 0; custom_idx < 4; custom_idx++) {
		uint32_t def = dtimg_get_default_custom(p, custom_idx);

		if (def)
			fprintf(fp, "  custom%d=0x%x\n", custom_idx, def);
	}

	fprintf(fp, "\n# entries");
	for (unsigned i = 0; i < be32_to_cpu(p->header.dt_entry_count); i++) {
		int fd_dtbo;

		fprintf(fp, "\nboard%d.dtbo\n", i + 1);

		dtimg_unpack_entry_section(fp, entry, id, "id", 0);
		dtimg_unpack_entry_section(fp, entry, rev, "rev", 0);
		dtimg_unpack_entry_section(fp, entry, custom[0], "custom0",
					   dtimg_get_default_custom(p, 0));
		dtimg_unpack_entry_section(fp, entry, custom[1], "custom1",
					   dtimg_get_default_custom(p, 1));
		dtimg_unpack_entry_section(fp, entry, custom[2], "custom2",
					   dtimg_get_default_custom(p, 2));
		dtimg_unpack_entry_section(fp, entry, custom[3], "custom3",
					   dtimg_get_default_custom(p, 3));

		snprintf(filename, sizeof(filename), "%s/board%d.dtbo",
			 outdir, i + 1);
		fd_dtbo = fileopen(filename, O_RDWR | O_CREAT | O_TRUNC, 0664);
		if (fd_dtbo < 0)
			return fd_dtbo;

		dd(fd, fd_dtbo, be32_to_cpu(entry->dt_offset), 0,
			be32_to_cpu(entry->dt_size), NULL, NULL);
		close(fd_dtbo);

		entry++;
	}

	fclose(fp);
	return 0;
}

static struct imgeditor dtimg_editor = {
	.name			= "adtimg",
	.descriptor		= "android mkdtimg editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct dt_table_header),
	.private_data_size	= sizeof(struct dtimg_private_data),
	.detect			= dtimg_detect,
	.list			= dtimg_list,
	.total_size		= dtimg_total_size,
	.summary		= dtimg_summary,
	.unpack			= dtimg_unpack,

	.search_magic		= {
		.magic		= dt_table_magic,
		.magic_sz	= sizeof(dt_table_magic),
		.magic_offset	= offsetof(struct dt_table_header, magic),
	}
};
REGISTER_IMGEDITOR(dtimg_editor);
