/*
 * allwinner sunxi_mbr/sunxi_dlinfo image editor.
 * (convert between sys_partition.fex and sunxi_mbr.fex)
 *
 * sys_partition.fex generate sunxi_mbr.fex and dlinfo.fex.
 * sunxi_mbr contains the partition informations like MBR, and dlinfo.fex
 * contains download informations.
 *
 * qianfan Zhao 2023-02-02
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "imgeditor.h"
#include "structure.h"
#include "string_helper.h"

#define PRINT_LEVEL0					"%-24s: "
#define PRINT_LEVEL1					"    %-20s: "
#define PRINT_LEVEL3					"        %-16s: "

#define SUNXI_MBR_COPY_NUMBER				4
#define SUNXI_MBR_FIRST_PARTITION_OFFSET		SIZE_MB(16)

#define SUNXI_MBR_MAGIC					"softw411"
#define SUNXI_MBR_MAX_PART_COUNTS			120

/* the unit of sunxi_mbr_partition.addr and len is SECTOR_SIZE */
#define SECTOR_SIZE					512

struct sunxi_mbr_partition {
	__le32						addr_hi;
	__le32						addr_lo;
	__le32						len_hi;
	__le32						len_lo;
	char						classname[16];
	char						name[16];
	__le32						type;
	__le32						keydata;
	__le32						ro;
	__le32						sig_verify;
	__le32						sig_erase;
	__le32						sig_value[4];
	__le32						sig_pubkey;
	__le32						sig_pubmode;

	/* this section must be clear as zero when write sunxi_mbr,
	 * we use it save download informations temporary.
	 */
	union {
		uint8_t					must_be_zero[36];
		struct {
			char				download_file[16];
			uint8_t				download_without_verify;
			uint8_t				download_sign;
		};
	};
};

static uint64_t make_uint64(uint32_t msb, uint32_t lsb)
{
	uint64_t u64 = msb;

	u64 <<= 32;
	u64 += lsb;

	return u64;
}

static const char *smart_size(uint64_t sector_addr, char *buf, size_t bufsz)
{
	return smart_format_size(sector_addr * SECTOR_SIZE, buf, bufsz);
}

static void structure_sunxi_mbr_partition_print_addr(const char *print_name_fmt,
						     const char *name,
						     const void *addr, size_t sz)
{
	const struct sunxi_mbr_partition *part =
			addr - offsetof(struct sunxi_mbr_partition, addr_hi);
	uint64_t sector_addr;
	char s[32];

	sector_addr = make_uint64(le32_to_cpu(part->addr_hi),
				  le32_to_cpu(part->addr_lo));

	printf(print_name_fmt, "addr");
	printf("0x%08" PRIx64 "(%8s)\n",
		sector_addr, smart_size(sector_addr, s, sizeof(s)));
}

static void structure_sunxi_mbr_partition_print_len(const char *print_name_fmt,
						     const char *name,
						     const void *addr, size_t sz)
{
	const struct sunxi_mbr_partition *part =
			addr - offsetof(struct sunxi_mbr_partition, len_hi);
	uint64_t sector_addr;
	char s[32];

	sector_addr = make_uint64(le32_to_cpu(part->len_hi),
				  le32_to_cpu(part->len_lo));

	printf(print_name_fmt, "len");
	printf("0x%08" PRIx64 "(%8s)\n",
		sector_addr, smart_size(sector_addr, s, sizeof(s)));
}

static const struct structure_item structure_sunxi_mbr_partition[] = {
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	addr_hi,	structure_sunxi_mbr_partition_print_addr),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	len_hi,		structure_sunxi_mbr_partition_print_len),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	classname,	structure_item_print_string),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	name,		structure_item_print_string),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	type,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	keydata,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	ro,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	sig_verify,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	sig_erase,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	sig_value,	structure_item_print_x32_array),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	sig_pubkey,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr_partition,	sig_pubmode,	structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

struct sunxi_mbr {
	union {
		/* sunxi_mbr are padding to 16K */
		uint8_t buffer[16 * 1024];

		struct {
			__le32				crc;
			__le32				version;
			char				magic[8];
			__le32				copy;
			__le32				index;
			__le32				part_counts;
			__le32				unused;
			struct sunxi_mbr_partition	partitions[SUNXI_MBR_MAX_PART_COUNTS];
			__le32				lockflag;
		};
	};
};

static const struct structure_item structure_sunxi_mbr[] = {
	STRUCTURE_ITEM(struct sunxi_mbr,	version,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr,	magic,		structure_item_print_string),
	STRUCTURE_ITEM(struct sunxi_mbr,	copy,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr,	index,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr,	part_counts,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_mbr,	lockflag,	structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

struct sunxi_dlpart {
	char						name[16];
	__le32						addr_hi;
	__le32						addr_lo;
	__le32						len_hi;
	__le32						len_lo;
	char						download_filename[16];
	char						verify_filename[16];
	__le32						encrypt;
	__le32						verify;
};

static void structure_sunxi_dlpart_print_addr(const char *print_name_fmt,
					      const char *name,
					      const void *addr, size_t sz)
{
	const struct sunxi_dlpart *part =
			addr - offsetof(struct sunxi_dlpart, addr_hi);
	uint64_t sector_addr;
	char s[32];

	sector_addr = make_uint64(le32_to_cpu(part->addr_hi),
				  le32_to_cpu(part->addr_lo));

	printf(print_name_fmt, "addr");
	printf("0x%08" PRIx64 "(%8s)\n",
		sector_addr, smart_size(sector_addr, s, sizeof(s)));
}

static void structure_sunxi_dlpart_print_len(const char *print_name_fmt,
					     const char *name,
					     const void *addr, size_t sz)
{
	const struct sunxi_dlpart *part =
			addr - offsetof(struct sunxi_dlpart, len_hi);
	uint64_t sector_addr;
	char s[32];

	sector_addr = make_uint64(le32_to_cpu(part->len_hi),
				  le32_to_cpu(part->len_lo));

	printf(print_name_fmt, "len");
	printf("0x%08" PRIx64 "(%8s)\n",
		sector_addr, smart_size(sector_addr, s, sizeof(s)));
}

static const struct structure_item structure_sunxi_dlpart[] = {
	STRUCTURE_ITEM(struct sunxi_dlpart,	addr_hi,		structure_sunxi_dlpart_print_addr),
	STRUCTURE_ITEM(struct sunxi_dlpart,	len_hi,			structure_sunxi_dlpart_print_len),
	STRUCTURE_ITEM(struct sunxi_dlpart,	download_filename,	structure_item_print_string),
	STRUCTURE_ITEM(struct sunxi_dlpart,	verify_filename,	structure_item_print_string),
	STRUCTURE_ITEM(struct sunxi_dlpart,	encrypt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_dlpart,	verify,			structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

struct sunxi_dlinfo {
	union {
		/* sunxi dlinfo are padding to 16K */
		uint8_t buffer[16 * 1024];

		struct {
			__le32				crc;
			__le32				version;
			char				magic[8];
			__le32				download_counts;
			__le32				stamp[3];
			struct sunxi_dlpart		download_parts[SUNXI_MBR_MAX_PART_COUNTS];
		};
	};
};

static const struct structure_item structure_sunxi_dlinfo[] = {
	STRUCTURE_ITEM(struct sunxi_dlinfo,	version,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct sunxi_dlinfo,	magic,			structure_item_print_string),
	STRUCTURE_ITEM(struct sunxi_dlinfo,	download_counts,	structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

struct sunxi_mbr_editor_private_data {
	struct sunxi_mbr				mbr;
	struct sunxi_dlinfo				dlinfo;
};

/* convert sunxi_mbr partition index to mmcblk part number.
 *
 * Ref:
 * u-boot-2014.07/board/sunxi/common/board_common.c: __set_part_name_for_sdmmc
 */
static uint32_t sunxi_mbr_partition_index_to_mmcblk(uint32_t index,
						    uint32_t part_counts)
{
	if (index == 0)
		return 2;
	else if (index + 1 == part_counts)
		return 1;

	return index + 4;
}

static int sunxi_mbr_detect(void *private_data, int force_type, int fd)
{
	struct sunxi_mbr_editor_private_data *p = private_data;
	struct sunxi_mbr *mbr = &p->mbr;
	uint32_t c32;
	int ret;

	static_assert(sizeof(struct sunxi_mbr_partition) == 128, "mbr");

	ret = read(fd, mbr, sizeof(*mbr));
	if (ret < 0)
		return ret;

	if (memcmp(mbr->magic, SUNXI_MBR_MAGIC, sizeof(mbr->magic)) != 0) {
		fprintf_if_force_type("Error: sunxi-mbr: bad magic\n");
		return -1;
	}

	c32 = crc32(0, ((uint8_t *)mbr) + sizeof(mbr->crc),
		    sizeof(*mbr) - sizeof(mbr->crc));
	if (c32 != mbr->crc) {
		fprintf_if_force_type("Error: sunxi-mbr: crc doesn't match\n");
		return -1;
	}

	return 0;
}

static int sunxi_mbr_list_verbose(struct sunxi_mbr *mbr)
{
	structure_print(PRINT_LEVEL0, mbr, structure_sunxi_mbr);

	printf("Partitions:");
	for (uint32_t i = 0; i < le32_to_cpu(mbr->part_counts); i++) {
		struct sunxi_mbr_partition *part = &mbr->partitions[i];

		printf("\n");
		structure_print(PRINT_LEVEL1, part, structure_sunxi_mbr_partition);
	}

	return 0;
}

static int sunxi_mbr_list(void *private_data, int fd, int argc, char **argv)
{
	struct sunxi_mbr_editor_private_data *p = private_data;
	struct sunxi_mbr *mbr = &p->mbr;
	int verbose = 0;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "verbose"))
			++verbose;
	}

	if (verbose)
		return sunxi_mbr_list_verbose(mbr);

	printf("      %8s %8s %-16s %s\n", "offset", "size", "name", "mmcblk");
	for (uint32_t i = 0; i < le32_to_cpu(mbr->part_counts); i++) {
		struct sunxi_mbr_partition *part = &mbr->partitions[i];
		uint64_t addr, len;
		char s[32];

		addr = make_uint64(le32_to_cpu(part->addr_hi),
				   le32_to_cpu(part->addr_lo));
		len = make_uint64(le32_to_cpu(part->len_hi),
				  le32_to_cpu(part->len_lo));

		printf("[%02d]: ", i + 1);
		printf("%8s ", smart_size(addr, s, sizeof(s)));
		printf("%8s ", smart_size(len, s, sizeof(s)));
		printf("%-16s ", part->name);
		printf("mmcblk0p%d",
			sunxi_mbr_partition_index_to_mmcblk(i,
					le32_to_cpu(mbr->part_counts)));
		printf("\n");
	}

	return 0;
}

static int sunxi_mbr_unpack(void *private_data, int fd, const char *outfile, int argc, char **argv)
{
	struct sunxi_mbr_editor_private_data *p = private_data;
	struct sunxi_mbr *mbr = &p->mbr;
	FILE *fp_out = fopen(outfile, "w+");

	if (!fp_out) {
		fprintf(stderr, "Error: open %s failed: %m\n", outfile);
		return -1;
	}

	fprintf(stderr, "Waring: sunxi mbr doesn't contain \"downloadfile\" property\n");
	fprintf(stderr, "        do not overwrite sys_partition.fex, otherwise you will lose \"downloadfile\"\n");

	fprintf(fp_out, "[mbr]\n");
	fprintf(fp_out, "size = %zu\n", sizeof(*mbr));
	fprintf(fp_out, "\n");

	fprintf(fp_out, "[partition_start]\n");
	fprintf(fp_out, "\n");

	for (uint32_t i = 0; i < le32_to_cpu(mbr->part_counts); i++) {
		struct sunxi_mbr_partition *part = &mbr->partitions[i];

		fprintf(fp_out, "[partition]\n");
		fprintf(fp_out, "\tname = %s\n", part->name);
		fprintf(fp_out, "\tsize = %d\n", le32_to_cpu(part->len_lo));
		fprintf(fp_out, "\tuser_type = 0x%x\n", le32_to_cpu(part->type));

		if (le32_to_cpu(part->keydata))
			fprintf(fp_out, "\tkeydata = %d\n", le32_to_cpu(part->keydata));
		fprintf(fp_out, "\n");
	}

	fclose(fp_out);
	return 0;
}

static int sunxi_mbr_partition_set_property(struct sunxi_mbr_partition *part,
					    const char *property,
					    const char *value)
{
	int ret = 0;
	uint32_t u32;

	if (!strcmp(property, "name")) {
		snprintf(part->name, sizeof(part->name), "%s", value);
	} else if (!strcmp(property, "size")) { /* size = 14336 */
		u32 = (uint32_t)strtol(value, NULL, 10);
		part->len_lo = cpu_to_le32(u32);

		/* the partition size can not be zero except UDISK */
		if (!u32 && strcmp(part->name, "UDISK"))
			ret = -1;
	} else if (!strcmp(property, "verify")) { /* verify = 0 */
		/* verify is the default feature. */
		u32 = (uint32_t)strtol(value, NULL, 10);
		if (u32 == 0)
			part->download_without_verify = 1;
	} else if (!strcmp(property, "downloadfile")) { /* downloadfifle = "abc" */
		const char *p = value + 1; /* skip the first " */
		int len = strlen(value) - 2; /* remove two " */

		if (value[0] != '"' || value[strlen(value) - 1] != '"') {
			ret = -1;
			goto done;
		} else if (len <= 0) {
			/* ignore downloadfile = "" */
			goto done;
		}

		/* let's save the downloadfile to mbr->unused part
		 * clear mbr->unused before write sunxi_mbr.fex.
		 */
		if (len > (int)sizeof(part->download_file))
			len = sizeof(part->download_file);

		memset(part->download_file, '0', sizeof(part->download_file));
		for (int i = 0; i < len; i++) {
			if (p[i] == '.')
				part->download_file[i] = '_';
			else if (p[i] >= 'a' && p[i] <= 'z')
				part->download_file[i] = toupper(p[i]);
			else
				part->download_file[i] = p[i];
		}
	} else if (!strcmp(property, "user_type")) { /* user_type = 0x8000 */
		u32 = (uint32_t)strtol(value, NULL, 16);
		part->type = cpu_to_le32(u32);
	} else {
		ret = -1;
	}

done:
	if (ret < 0) {
		fprintf(stderr, "parse ini failed when %s = %s\n",
			property, value);
	}

	return ret;
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

static int sunxi_mbr_save(int fd_outimg, struct sunxi_mbr_editor_private_data *p)
{
	struct sunxi_dlinfo *dlinfo = &p->dlinfo;
	struct sunxi_mbr *mbr = &p->mbr;
	int dlpart_count = 0;

	char dlinfo_filepath[1024];
	int part_counts = le32_to_cpu(mbr->part_counts);
	uint32_t offset = SUNXI_MBR_FIRST_PARTITION_OFFSET / SECTOR_SIZE;
	int udisk_exist = 0;
	uint32_t c32;

	for (int i = 0; i < part_counts; i++) {
		struct sunxi_mbr_partition *part = &mbr->partitions[i];

		udisk_exist |= (!strcmp(part->name, "UDISK"));
	}

	/* make sure UDISK partition is exist */
	if (!udisk_exist) {
		struct sunxi_mbr_partition *udisk = &mbr->partitions[part_counts];

		++part_counts;
		snprintf(udisk->name, sizeof(udisk->name), "UDISK");
	}

	memcpy(mbr->magic, SUNXI_MBR_MAGIC, sizeof(mbr->magic));
	mbr->part_counts = cpu_to_le32(part_counts);
	mbr->version = cpu_to_le32(512);
	mbr->copy = cpu_to_le32(4);

	/* the fix logic is based on allwinner bsp
	 * brandy/pack_tools/create_mbr/update_mbr.c:update_for_part_info
	 */
	for (int i = 0; i < part_counts; i++) {
		struct sunxi_mbr_partition *part = &mbr->partitions[i];
		struct sunxi_dlpart *dlpart = &dlinfo->download_parts[dlpart_count];

		part->addr_lo = cpu_to_le32(offset);
		offset += le32_to_cpu(part->len_lo);

		snprintf(part->classname, sizeof(part->classname), "DISK");

		if (le32_to_cpu(part->sig_verify) == 0) {
			if (!strcmp(part->name, "boot") || !strcmp(part->name, "system"))
				part->sig_verify = cpu_to_le32(0x8000);
		}

		if (le32_to_cpu(part->sig_erase) == 0) {
			if (!strcmp(part->name, "data")
			    || !strcmp(part->name, "cache")
			    || !strcmp(part->name, "UDISK")
			    || !strcmp(part->name, "private")
			    || !strcmp(part->name, "drm"))
				part->sig_erase = cpu_to_le32(0x8000);
		}

		if (part->download_file[0] != '\0') { /* downloadfile */
			dlpart->addr_hi = part->addr_hi;
			dlpart->addr_lo = part->addr_lo;
			dlpart->len_hi = part->len_hi;
			dlpart->len_lo = part->len_lo;
			dlpart->verify = cpu_to_le32(!part->download_without_verify);
			memcpy(dlpart->name, part->name, sizeof(dlpart->name));
			memcpy(dlpart->download_filename, part->download_file,
				sizeof(dlpart->download_filename));

			if (!part->download_without_verify) {
				dlpart->verify_filename[0] = 'V';
				memcpy(&dlpart->verify_filename[1],
					part->download_file,
					sizeof(dlpart->verify_filename) - 1);
			}
			dlpart_count++;
		}

		memset(part->must_be_zero, 0, sizeof(part->must_be_zero));
	}

	for (int i = 0; i < SUNXI_MBR_COPY_NUMBER; i++) {
		mbr->index = cpu_to_le32(i);

		c32 = crc32(0, (uint8_t *)mbr + sizeof(mbr->crc),
			    sizeof(*mbr) - sizeof(mbr->crc));
		mbr->crc = cpu_to_le32(c32);
		write(fd_outimg, mbr, sizeof(*mbr));

	}

	/* let's save dlinfo file */
	dlinfo->version = cpu_to_le32(512);
	dlinfo->download_counts = cpu_to_le32(dlpart_count);
	memcpy(dlinfo->magic, SUNXI_MBR_MAGIC, sizeof(dlinfo->magic));
	c32 = crc32(0, (uint8_t *)dlinfo + sizeof(dlinfo->crc),
		    sizeof(*dlinfo) - sizeof(dlinfo->crc));
	dlinfo->crc = cpu_to_le32(c32);

	if (get_filepath_byfd(fd_outimg, dlinfo_filepath, sizeof(dlinfo_filepath)) < 0) {
		fprintf(stderr, "Waring: auto detect dlinfo filepath failed\n");
	} else {
		int fd;

		strncat(dlinfo_filepath, ".dlinfo", sizeof(dlinfo_filepath));
		fd = fileopen(dlinfo_filepath, O_RDWR | O_CREAT, 0664);
		if (!(fd < 0)) {
			write(fd, dlinfo, sizeof(*dlinfo));
			close(fd);
		}
	}

	return 0;
}

#define INI_PARTITION		"[partition]"

/* write both sunxi_mbr.fex and dlinfo.fex */
static int sunxi_mbr_pack(void *private_data, const char *sys_partition_ini,
			  int fd_outimg, int argc, char **argv)
{
	struct sunxi_mbr_editor_private_data *p = private_data;
	struct sunxi_mbr *mbr = &p->mbr;

	FILE *fp = fopen(sys_partition_ini, "r");
	int line_number = 0, partition_idx = -1;
	char prev_section[128] = { 0 };
	char buf[1024];
	int ret = 0;

	if (!fp) {
		fprintf(stderr, "Error: open %s failed: %m\n", sys_partition_ini);
		return -1;
	}

	while (fgets(buf, sizeof(buf) - 1, fp)) {
		/* strings after ";" is comments and will be ignored */
		char *comment = strchr(buf, ';');
		char *key, *value, *eq;

		line_number++;

		if (comment)
			*comment = '\0';

		key = string_skip_head_space(buf);
		string_remove_eol_space(buf);
		if (*key == '\0') /* skip empty line */
			continue;

		if (key[0] == '[') { /* it is section */
			strncpy(prev_section, key, sizeof(prev_section) - 1);
			if (!strcmp(prev_section, INI_PARTITION)) {
				if (++partition_idx > SUNXI_MBR_MAX_PART_COUNTS) {
					fprintf(stderr, "Error: part counts overflow\n");
					return -1;
				}
			}

			continue;
		}

		/* the next is "key = value" string */
		eq = strchr(key, '=');
		if (!eq) {
			fprintf(stderr, "Error: %04d: parse failed\n",
				line_number);
			ret = -1;
			break;
		}

		/* remove all the space after "key" */
		*eq = '\0';
		string_remove_eol_space(key);
		value = string_skip_head_space(eq + 1);

		if (!strcmp(prev_section, INI_PARTITION)) {
			ret = sunxi_mbr_partition_set_property(&mbr->partitions[partition_idx],
							       key, value);
			if (ret < 0)
				break;
		}
	}

	fclose(fp);

	if (partition_idx < 0) {
		fprintf(stderr, "Error: no partition found\n");
		ret = -1;
	} else if (!ret) {
		mbr->part_counts = cpu_to_le32(partition_idx + 1);
		ret = sunxi_mbr_save(fd_outimg, p);
	}

	return ret;
}

struct sunxi_dlinfo_editor_private_data {
	struct sunxi_dlinfo		dlinfo;
};

static int sunxi_dlinfo_detect(void *private_data, int force_type, int fd)
{
	struct sunxi_dlinfo_editor_private_data *p = private_data;
	struct sunxi_dlinfo *dlinfo = &p->dlinfo;
	uint32_t c32;
	int ret;

	/* sunxi dlinfo has the same magic and crc method as mbr,
	 * the detect function only work when '--type sunxi_dlinfo' is selected.
	 */
	if (!force_type)
		return -1;

	ret = read(fd, dlinfo, sizeof(*dlinfo));
	if (ret < 0)
		return ret;

	if (memcmp(dlinfo->magic, SUNXI_MBR_MAGIC, sizeof(dlinfo->magic)) != 0) {
		fprintf_if_force_type("Error: sunxi_dlinfo: bad magic\n");
		return -1;
	}

	c32 = crc32(0, ((uint8_t *)dlinfo) + sizeof(dlinfo->crc),
		    sizeof(*dlinfo) - sizeof(dlinfo->crc));
	if (c32 != dlinfo->crc) {
		fprintf_if_force_type("Error: sunxi_dlinfo: crc doesn't match\n");
		return -1;
	}

	return 0;
}

static int sunxi_dlinfo_list(void *private_data, int fd, int argc, char **argv)
{
	struct sunxi_dlinfo_editor_private_data *p = private_data;
	struct sunxi_dlinfo *dlinfo = &p->dlinfo;

	structure_print(PRINT_LEVEL0, dlinfo, structure_sunxi_dlinfo);

	printf("Downloads:");
	for (uint32_t i = 0; i < le32_to_cpu(dlinfo->download_counts); i++) {
		struct sunxi_dlpart *part = &dlinfo->download_parts[i];

		printf("\n");
		structure_print(PRINT_LEVEL1, part, structure_sunxi_dlpart);
	}

	return 0;
}

static struct imgeditor sunxi_mbr_editor = {
	.name			= "sunxi_mbr",
	.descriptor		= "allwinner sunxi_mbr image editor",
	.header_size		= sizeof(struct sunxi_mbr),
	.private_data_size	= sizeof(struct sunxi_mbr_editor_private_data),
	.detect			= sunxi_mbr_detect,
	.unpack			= sunxi_mbr_unpack,
	.pack			= sunxi_mbr_pack,
	.list			= sunxi_mbr_list,
};
REGISTER_IMGEDITOR(sunxi_mbr_editor);

static struct imgeditor sunxi_dlinfo_editor = {
	.name			= "sunxi_dlinfo",
	.descriptor		= "allwinner dlinfo.fex image editor",
	.header_size		= sizeof(struct sunxi_dlinfo),
	.private_data_size	= sizeof(struct sunxi_dlinfo_editor_private_data),
	.detect			= sunxi_dlinfo_detect,
	.list			= sunxi_dlinfo_list,
};
REGISTER_IMGEDITOR(sunxi_dlinfo_editor);

