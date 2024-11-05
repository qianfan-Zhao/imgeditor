/*
 * for android mkdtimg
 *
 * qianfan Zhao <qianfanguijin@163.com
 */

#include <stdlib.h>
#include <string.h>
#include "imgeditor.h"
#include "structure.h"
#include "exini.h"
#include "string_helper.h"
#include "fdt/fdt_export.h"

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

static int dtimg_dtbo_get_global(struct fdt_editor_private_data *fdt,
				 uint32_t *ret_board_id,
				 uint32_t *ret_board_rev)
{
	struct device_node *node;

	*ret_board_id = 0;
	*ret_board_rev = 0;

	node = device_node_find_byname(fdt->root, "board_id");
	if (node)
		device_node_read_u32(node, ret_board_id);

	node = device_node_find_byname(fdt->root, "board_rev");
	if (node)
		device_node_read_u32(node, ret_board_rev);

	return 0;
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
	int ret = 0;
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/dtboimg.cfg", outdir);
	fp = fopen(filename, "w+");
	if (!fp) {
		fprintf(stderr, "Error: create %s failed\n", filename);
		return -1;
	}

	fprintf(fp, "# global options\n");
	fprintf(fp, "  id=/:board_id\n");
	fprintf(fp, "  rev=/:board_rev\n");

	for (int custom_idx = 0; custom_idx < 4; custom_idx++) {
		uint32_t def = dtimg_get_default_custom(p, custom_idx);

		if (def)
			fprintf(fp, "  custom%d=0x%x\n", custom_idx, def);
	}

	fprintf(fp, "\n# entries");
	for (unsigned i = 0; i < be32_to_cpu(p->header.dt_entry_count); i++) {
		struct fdt_editor_private_data fdt = { 0 };
		uint32_t board_id, board_rev;
		int vfd, fd_dtbo;

		vfd = virtual_file_dup(fd, be32_to_cpu(entry->dt_offset));
		if (!vfd) {
			ret = vfd;
			break;
		}

		ret = fdt_editor_detect(&fdt, 1, vfd);
		if (ret < 0) {
			virtual_file_close(vfd);
			break;
		}

		fprintf(fp, "\nboard%d.dtbo\n", i + 1);
		dtimg_dtbo_get_global(&fdt, &board_id, &board_rev);

		dtimg_unpack_entry_section(fp, entry, id, "id", board_id);
		dtimg_unpack_entry_section(fp, entry, rev, "rev", board_rev);
		dtimg_unpack_entry_section(fp, entry, custom[0], "custom0",
					   dtimg_get_default_custom(p, 0));
		dtimg_unpack_entry_section(fp, entry, custom[1], "custom1",
					   dtimg_get_default_custom(p, 1));
		dtimg_unpack_entry_section(fp, entry, custom[2], "custom2",
					   dtimg_get_default_custom(p, 2));
		dtimg_unpack_entry_section(fp, entry, custom[3], "custom3",
					   dtimg_get_default_custom(p, 3));

		fdt_editor_exit(&fdt);
		virtual_file_close(vfd);

		snprintf(filename, sizeof(filename), "%s/board%d.dtbo",
			 outdir, i + 1);
		fd_dtbo = fileopen(filename, O_RDWR | O_CREAT | O_TRUNC, 0664);
		if (fd_dtbo < 0) {
			ret = fd_dtbo;
			break;
		}

		dd(fd, fd_dtbo, be32_to_cpu(entry->dt_offset), 0,
			be32_to_cpu(entry->dt_size), NULL, NULL);
		close(fd_dtbo);

		entry++;
	}

	fclose(fp);
	return ret;
}

static unsigned long dtimg_entry_read_ulong(struct exini_section *global_option,
					    struct exini_section *section,
					    const char *id,
					    int *err)
{
	unsigned long n = 0;

	n = exini_section_read_ulong(section, id, n, err);
	if (*err == 0)
		return n;

	return exini_section_read_ulong(global_option, id, n, err);
}

/* # global options
 *  id=/:board_id
 *  rev=/:board_rev
 *  custom0=0xabc
 */
static int dtimg_init_entry_from_dt(struct fdt_editor_private_data *fdt,
				    struct exini_section *global_option,
				    struct exini_section *section,
				    const char *id,
				    __be32 *ret_number)
{
	struct device_node *node;
	char nonconst_fdtpath[128];
	const char *fdtpath;
	uint32_t n;
	int err;

	fdtpath = exini_section_read_string(section, id, NULL, 1, &err);
	if (fdtpath)
		goto skip_global_option;

	fdtpath = exini_section_read_string(global_option, id, NULL, 1, &err);
	if (err == EXINI_ERR_BAD_TYPE) {
		fprintf(stderr, "Error: read global option's %s "
			"failed with bad type\n",
			id);
		return -err;
	}

	if (!fdtpath) /* no such configuration */
		return 0;

skip_global_option:
	strncpy(nonconst_fdtpath, fdtpath, sizeof(nonconst_fdtpath) - 1);
	string_replace_in_buffer(nonconst_fdtpath, sizeof(nonconst_fdtpath),
				":", "");

	node = device_node_find_bypath(fdt->root, nonconst_fdtpath);
	if (!node) /* it's OK if this value is not defined in fdt */
		return 0;

	err = device_node_read_u32(node, &n);
	if (err < 0) {
		fprintf(stderr, "Error: %s in fdt is not a number\n",
			nonconst_fdtpath);
		return err;
	}

	*ret_number = cpu_to_be32(n);
	return 0;
}

static int dtimg_init_entry(int fd_dtb, struct dt_table_entry *entry,
			    struct exini_section *global_option,
			    struct exini_section *section)
{
	struct fdt_editor_private_data fdt = { 0 };
	int err_id = 0, err_rev = 0, err = 0;
	int ret;
	unsigned long n;

	ret = fdt_editor_detect(&fdt, 1, fd_dtb);
	if (ret < 0)
		return ret;

	/* try id first */
	n = dtimg_entry_read_ulong(global_option, section, "id", &err_id);
	if (err_id == 0) {
		entry->id = cpu_to_be32(n);
	} else if (err_id == EXINI_ERR_BAD_TYPE) {
		ret = dtimg_init_entry_from_dt(&fdt, global_option, section,
					       "id", &entry->id);
		if (ret < 0)
			goto done;
	}

	n = dtimg_entry_read_ulong(global_option, section, "rev", &err_rev);
	if (err_rev == 0) {
		entry->rev = cpu_to_be32(n);
	} else if (err_rev == EXINI_ERR_BAD_TYPE) {
		ret = dtimg_init_entry_from_dt(&fdt, global_option, section,
					       "rev", &entry->rev);
		if (ret < 0)
			goto done;
	}

	for (unsigned i = 0; i < 4; i++) {
		char id[32];

		snprintf(id, sizeof(id), "custom%d", i);
		n = dtimg_entry_read_ulong(global_option, section, id, &err);
		if (err == EXINI_ERR_BAD_TYPE) {
			fprintf(stderr, "Error: %s/%s is not number\n",
				section->name, id);
			return -err;
		}

		entry->custom[i] = cpu_to_be32(n);
	}

done:
	fdt_editor_exit(&fdt);
	return ret;
}

static int dtimg_pack(void *private_data, const char *dir, int fd_outimg,
		      int argc, char **argv)
{
	struct dtimg_private_data *p = private_data;
	struct dt_table_entry *entries;
	struct exini dtbocfg = { .enable_unknown_data = 1 };
	struct exini_section *section, *global_option;
	uint32_t data_offset = 0;
	char filename[1024];
	FILE *fp;
	int ret;

	snprintf(filename, sizeof(filename), "%s/dtboimg.cfg", dir);
	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Error: open %s failed\n", filename);
		return -1;
	}

	ret = exini_load(&dtbocfg, fp, EXINI_QUIRK_DTBOCFG);
	fclose(fp);
	if (ret < 0)
		return ret;

	global_option = exini_find_section(&dtbocfg, "__global_option");
	if (!global_option) {
		fprintf(stderr, "Error: global option is not found\n");
		ret = -1;
		goto done;
	}

	p->entries = calloc(dtbocfg.total_sections - 1, /* not include global option */
			    sizeof(struct dt_table_entry));
	if (!p->entries) {
		fprintf(stderr, "Error: alloc %zu entries failed\n",
			dtbocfg.total_sections - 1);
		ret = -1;
		goto done;
	}

	p->header.magic = cpu_to_be32(DT_TABLE_MAGIC);
	p->header.header_size = cpu_to_be32(sizeof(struct dt_table_header));
	p->header.dt_entry_count = cpu_to_be32(dtbocfg.total_sections - 1);
	p->header.dt_entry_size = cpu_to_be32(sizeof(struct dt_table_entry));
	p->header.dt_entries_offset = cpu_to_be32(sizeof(struct dt_table_header));
	p->header.page_size = cpu_to_be32(DT_TABLE_DEFAULT_PAGE_SIZE);
	p->header.version = cpu_to_be32(DT_TABLE_DEFAULT_VERSION);

	entries = p->entries;
	data_offset += be32_to_cpu(p->header.header_size);
	data_offset += be32_to_cpu(p->header.dt_entry_size)
		* be32_to_cpu(p->header.dt_entry_count);

	list_for_each_entry(section, &dtbocfg.sections, head,
			    struct exini_section) {
		/* -1: not include global option */
		struct dt_table_entry *entry = &entries[section->global_index - 1];
		uint32_t length;
		int fd_dtb;

		if (!strcmp(section->name, "__global_option"))
			continue;

		entry->dt_offset = cpu_to_be32(data_offset);

		snprintf(filename, sizeof(filename), "%s/%s", dir, section->name);
		fd_dtb = fileopen(filename, O_RDONLY, 0);
		if (fd_dtb < 0) {
			ret = fd_dtb;
			break;
		}

		ret = dtimg_init_entry(fd_dtb, entry, global_option, section);
		if (ret < 0) {
			fprintf(stderr, "Error: init %s entry failed\n",
				section->name);
			close(fd_dtb);
			break;
		}

		length = (uint32_t)filelength(fd_dtb);
		entry->dt_size = cpu_to_be32(length);
		dd(fd_dtb, fd_outimg, 0, data_offset, length, NULL, NULL);
		close(fd_dtb);

		data_offset += length;
	}

	p->header.total_size = cpu_to_be32(data_offset);

	fileseek(fd_outimg, 0);
	write(fd_outimg, &p->header, sizeof(p->header));
	write(fd_outimg, p->entries,
		be32_to_cpu(p->header.dt_entry_size)
		* be32_to_cpu(p->header.dt_entry_count));

done:
	exini_free(&dtbocfg);
	return ret;
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
	.pack			= dtimg_pack,

	.search_magic		= {
		.magic		= dt_table_magic,
		.magic_sz	= sizeof(dt_table_magic),
		.magic_offset	= offsetof(struct dt_table_header, magic),
	}
};
REGISTER_IMGEDITOR(dtimg_editor);
