/*
 * EFI GUID Partition Table
 * qianfan Zhao
 */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include "imgeditor.h"
#include "structure.h"
#include "string_helper.h"
#include "json_helper.h"
#include "libxopt.h"

/*
 * The earliest LBA this can be at is LBA#2 (i.e. right behind
 * the (protective) MBR and the GPT header.
 */
#define GPT_DEF_PART_ENTRY_LBA		2
#define GPT_DEF_NUM_PARTS		56	/* as same as u-boot */
#define GPT_MAX_NUM_PARTS		128

#define GPT_PARTS_RESERVED_LBA		\
	((GPT_MAX_NUM_PARTS * sizeof(struct gpt_entry)) / LBA_BLKSZ)
#define GPT_DEF_FIRST_LBA		\
	(GPT_DEF_PART_ENTRY_LBA + GPT_PARTS_RESERVED_LBA)

#define DOS_PART_TBL_OFFSET		0x1be
#define DOS_PART_MAGIC_OFFSET		0x1fe

struct dos_partition {
	__u8	boot_ind;	/* 0x80 - active			*/
	__u8	head;		/* starting head			*/
	__u8	sector;		/* starting sector			*/
	__u8	cyl;		/* starting cylinder			*/
	__u8	sys_ind;	/* What partition type			*/
	__u8	end_head;	/* end head				*/
	__u8	end_sector;	/* end sector				*/
	__u8	end_cyl;	/* end cylinder				*/
	__le32	start;		/* starting sector counting from 0	*/
	__le32	size;		/* nr of sectors in partition		*/
};

#define LBA_BLKSZ				512
#define lba2sz(lba)				((lba) * LBA_BLKSZ)

#define GPT_PRIMARY_PARTITION_TABLE_LBA		1

struct efi_guid {
	__u8 b[16];
};

static const char gpt_signature[] = "EFI PART";

#define GPT_HEADER_REVISION_V1 0x00010000

struct __attribute__((packed)) gpt_header {
	__le64			signature;
	__le32			revision;
	__le32			header_size;
	__le32			header_crc32;
	__le32			reserved1;
	__le64			my_lba;
	__le64			alternate_lba;
	__le64			first_usable_lba;
	__le64			last_usable_lba;
	struct efi_guid		disk_guid;
	__le64			partition_entry_lba;
	__le32			num_partition_entries;
	__le32			sizeof_partition_entry;
	__le32			partition_entry_array_crc32;
};

/*
 * UUID - Universally Unique IDentifier
 * 0        9    14   19   24
 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 *    be     be   be   be       be
 *
 * GUID - Globally Unique Identifier
 * 0        9    14   19   24
 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 *    le     le   le   be       be
 */
static const uint8_t uuid_char_order[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
	9, 10, 11, 12, 13, 14, 15
};

static const uint8_t guid_char_order[] = {
	3, 2, 1, 0, 5, 4, 7, 6,
	8, 9, 10, 11, 12, 13, 14, 15
};

static char *snprintf_uuid(char *buf, size_t sz, int isuuid, const char *char_fmt,
			   const struct efi_guid *guid)
{
	const uint8_t *order;
	char *p = buf;

	order = uuid_char_order;
	if (!isuuid)
		order = guid_char_order;

	for (size_t i = 0; i < sizeof(guid_char_order); i++) {
		uint8_t idx = order[i];

		snprintf_continue(&p, &sz, char_fmt, guid->b[idx]);

		switch (i) {
		case 3:
		case 5:
		case 7:
		case 9:
			snprintf_continue(&p, &sz, "-");
			break;
		}
	}

	return buf;
}

static int sscanf_uuid(struct efi_guid *guid, int isuuid, const char *s)
{
	const uint8_t *order = isuuid ? uuid_char_order : guid_char_order;
	uint8_t uuid[sizeof(*guid) + 1] = { 0 };
	int ret;

	ret = xstring(s, NULL, uuid, sizeof(uuid));
	if (ret != sizeof(*guid)) {
		fprintf(stderr, "Error: invalid uuid string %s\n", s);
		return -1;
	}

	for (size_t i = 0; i < sizeof(*guid); i++)
		guid->b[order[i]] = uuid[i];

	return 0;
}

static void structure_item_print_guid(const char *print_name_fmt, const char *name,
				      const void *addr, size_t sz)
{
	char guid[64];

	structure_print_name(print_name_fmt, name);
	snprintf_uuid(guid, sizeof(guid), 0, "%02x", addr);
	printf("%s\n", guid);
}

static int structure_item_save_guid(cJSON *root, const char *name,
				     const void *addr, size_t sz)
{
	char guid[64];

	snprintf_uuid(guid, sizeof(guid), 0, "%02x", addr);
	json_create_or_update_string_value(root, name, guid);

	return 0;
}

static int structure_item_load_guid(cJSON *root, const char *name,
				    void *addr, size_t sz)
{
	const char *s = json_get_string_value_in_object(root, name);

	if (!s)
		return -1;

	return sscanf_uuid(addr, 0, s);
}

static const struct structure_item structure_gpt_header[] = {
	STRUCTURE_ITEM(struct gpt_header, signature,			structure_item_print_string,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct gpt_header, revision,			structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct gpt_header, header_size,			structure_item_print_unsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct gpt_header, header_crc32,			structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct gpt_header, my_lba,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, alternate_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, first_usable_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, last_usable_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, disk_guid,			structure_item_print_guid,
			.save_json = structure_item_save_guid,
			.load_json = structure_item_load_guid),
	STRUCTURE_ITEM(struct gpt_header, partition_entry_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, num_partition_entries,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, sizeof_partition_entry,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, partition_entry_array_crc32,	structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM_END(),
};

union __attribute__((packed)) gpt_entry_attributes {
	struct {
		__u64 required_to_function:1;
		__u64 no_block_io_protocol:1;
		__u64 legacy_bios_bootable:1;
		__u64 reserved:45;
		__u64 type_guid_specific:16;
	} fields;
	unsigned long long raw;
};

#define PARTNAME_SZ	(72 / sizeof(__le16))

struct __attribute__((packed)) gpt_entry {
	struct efi_guid			partition_type_guid;
	struct efi_guid			unique_partition_guid;
	__le64				starting_lba;
	__le64				ending_lba;
	union gpt_entry_attributes	attributes;
	__le16				partition_name[PARTNAME_SZ];
};

static void gpt_partition_name_to_char(const __le16 *ws, char *buf, size_t bufsz)
{
	size_t i = 0;

	for (; i < PARTNAME_SZ && i < bufsz - 1; i++) {
		int c = le16_to_cpu(ws[i]);

		if (c == '\0')
			break;

		buf[i] = (char)c;
	}

	buf[i] = '\0';
}

static void set_gpt_partition_name(__le16 *ws, const char *s)
{
	for (size_t i = 0; i < PARTNAME_SZ - 1 && *s != '\0'; i++) {
		ws[i] = cpu_to_le16(*s);
		s++;
	}
}

static void structure_item_print_wchar_string(const char *print_name_fmt,
					      const char *name,
					      const void *addr,
					      size_t sz)
{
	char part_name[PARTNAME_SZ + 1];

	gpt_partition_name_to_char(addr, part_name, sizeof(part_name));

	structure_print_name(print_name_fmt, name);
	printf("%s\n", part_name);
	putchar('\n');
}

static int structure_item_save_wchar_string(cJSON *root, const char *name,
					    const void *addr, size_t sz)
{
	const __le16 *ws = addr;
	char tmps[sz];

	memset(tmps, 0, sz);

	for (size_t i = 0; i < sz / sizeof(__le16); i++) {
		int c = le16_to_cpu(ws[i]);

		tmps[i] = (char)c;
	}

	json_create_or_update_string_value(root, name, tmps);
	return 0;
}

static int structure_item_load_wchar_string(cJSON *root, const char *name,
					    void *addr, size_t sz)
{
	const char *s = json_get_string_value_in_object(root, name);
	__le16 *ws = addr;

	if (!s)
		return -1;

	set_gpt_partition_name(ws, s);
	return 0;
}

static const struct structure_item structure_gpt_entry[] = {
	STRUCTURE_ITEM(struct gpt_entry, partition_type_guid,		structure_item_print_guid,
			.save_json = structure_item_save_guid,
			.load_json = structure_item_load_guid),
	STRUCTURE_ITEM(struct gpt_entry, unique_partition_guid,		structure_item_print_guid,
			.save_json = structure_item_save_guid,
			.load_json = structure_item_load_guid),
	STRUCTURE_ITEM(struct gpt_entry, starting_lba,			structure_item_print_unsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct gpt_entry, ending_lba,			structure_item_print_unsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct gpt_entry, attributes,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct gpt_entry, partition_name,		structure_item_print_wchar_string,
			.save_json = structure_item_save_wchar_string,
			.load_json = structure_item_load_wchar_string),
	STRUCTURE_ITEM_END(),
};

struct gpt_write_partition_pdata {
	uint64_t		next_lba;
	uint32_t		active_partition_entries;
	struct efi_guid		disk_type_uid;
};

struct gpt_editor_private_data {
	int			fd;
	struct gpt_header	hdr;

	struct gpt_entry	*partitions;
	uint32_t		num_partition_entries;

	/* used for gpt_main */
	struct gpt_write_partition_pdata wpdata;
};

static struct gpt_entry *gpt_find_part(struct gpt_editor_private_data *p,
				       const char *part_name)
{
	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *e = &p->partitions[i];
		uint8_t entry_name[sizeof(e->partition_name)];
		char name[128] = { 0 };

		memcpy(entry_name, e->partition_name, sizeof(entry_name));
		gpt_partition_name_to_char((const __le16 *)entry_name,
					   name, sizeof(name));

		if (!strcmp(name, part_name))
			return e;
	}

	return NULL;
}

static void gpt_exit(void *private_data)
{
	struct gpt_editor_private_data *p = private_data;

	if (p->partitions)
		free(p->partitions);
}

static uint32_t gpt_header_calc_crc32(struct gpt_header *hdr)
{
	uint32_t c32, c32_backup = le32_to_cpu(hdr->header_crc32);

	hdr->header_crc32 = 0;
	c32 = crc32(0, (uint8_t *)hdr, sizeof(*hdr));
	hdr->header_crc32 = cpu_to_le32(c32_backup);

	return c32;
}

static int gpt_load_entries(struct gpt_editor_private_data *p,
			    uint64_t entry, int force_type, int fd)
{
	size_t partition_entries_sz;
	uint32_t c32;
	int ret;

	/* partition_entry_lba is a absolute address, we should use
	 * lseek64 instead of fileseek.
	 */
	lseek64(fd, entry, SEEK_SET);
	partition_entries_sz = le32_to_cpu(p->hdr.sizeof_partition_entry);
	partition_entries_sz *= p->num_partition_entries;
	ret = read(fd, p->partitions, partition_entries_sz);
	if (ret < 0)
		return ret;

	if ((size_t)ret != partition_entries_sz) {
		uint32_t entry_lba = le32_to_cpu(p->hdr.partition_entry_lba);
		uint64_t minimum_sz;
		char smart_sz[64];

		minimum_sz = lba2sz((uint64_t)entry_lba) + partition_entries_sz;
		smart_format_size(minimum_sz, smart_sz, sizeof(smart_sz));

		fprintf_if_force_type("Error: read whole partitons failed\n"
				      "       partition_entry_lba = %d\n"
				      "       num_partition_entries = %d\n"
				      "       sizeof_partition_entry = %d\n",
				      entry_lba,
				      p->num_partition_entries,
				      le32_to_cpu(p->hdr.sizeof_partition_entry)
				      );
		fprintf_if_force_type("The minimum size should be: "
				      "%" PRIu64 " Bytes (%s)\n",
				      minimum_sz, smart_sz);
		return -1;
	}

	c32 = crc32(0, (uint8_t *)p->partitions, partition_entries_sz);
	if (c32 != le32_to_cpu(p->hdr.partition_entry_array_crc32)) {
		fprintf_if_force_type("Error: partition_entry_array_crc32 doesn't match"
				      "(%08x != %08x)\n",
				      c32, p->hdr.partition_entry_array_crc32);
		return -1;
	}

	return 0;
}

static void gpt_register_disk_partitions(struct gpt_editor_private_data *p)
{
	struct disk_partitions *dp;
	size_t n_parts = 0;

	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];

		if (le32_to_cpu(entry->starting_lba) == 0)
			break;
		if (le32_to_cpu(entry->ending_lba) == 0)
			break;

		++n_parts;
	}

	dp = alloc_disk_partitions("GPT", n_parts);
	if (!dp)
		return;

	for (size_t i = 0; i < n_parts; i++) {
		struct disk_partition *part = &dp->parts[i];
		struct gpt_entry *entry = &p->partitions[i];
		uint8_t entry_name[sizeof(entry->partition_name)];

		part->start_addr = lba2sz(le32_to_cpu(entry->starting_lba));
		part->end_addr = lba2sz(le32_to_cpu(entry->ending_lba)) - 1;

		/* to avoid:
		 * error: taking address of packed member of ‘struct gpt_entry’
		 * may result in an unaligned pointer value
		 */
		memcpy(entry_name, entry->partition_name, sizeof(entry_name));
		gpt_partition_name_to_char((const __le16 *)entry_name,
					   part->name, sizeof(part->name));
	}

	register_disk_partitions(dp);
}

static int gpt_detect(void *private_data, int force_type, int fd)
{
	struct gpt_editor_private_data *p = private_data;
	int good_signature = 0;
	size_t gpt_offset = 0;
	uint32_t c32;
	int ret;

	static_assert(sizeof(struct gpt_header) == 92, "struct gpt_header");

	for (int i = 0; i < 2 && !good_signature; i++) {
		switch (i) {
		case 0:
			gpt_offset = 0;
			break;
		default:
			gpt_offset = lba2sz(GPT_PRIMARY_PARTITION_TABLE_LBA);
			break;
		}

		fileseek(fd, gpt_offset);
		ret = read(fd, &p->hdr, sizeof(p->hdr));
		if (ret < 0)
			return ret;

		good_signature = !memcmp(&p->hdr.signature,
					 gpt_signature,
					 strlen(gpt_signature));
	}

	if (!good_signature) {
		fprintf_if_force_type("Error: signature doesn't match\n");
		return -1;
	}

	if (le32_to_cpu(p->hdr.header_size) != sizeof(struct gpt_header)) {
		fprintf_if_force_type("Error: header_size doesn't match\n");
		return -1;
	}

	c32 = gpt_header_calc_crc32(&p->hdr);
	if (c32 != le32_to_cpu(p->hdr.header_crc32)) {
		fprintf_if_force_type("Error: header_crc32 doesn't match "
				      "(%08x != %08x)\n",
				      c32, p->hdr.header_crc32);
		return -1;
	}

	p->num_partition_entries = le32_to_cpu(p->hdr.num_partition_entries);
	p->partitions = calloc(p->num_partition_entries,
				le32_to_cpu(p->hdr.sizeof_partition_entry));
	if (!p->partitions) {
		fprintf(stderr, "Error: alloc %d partitons failed.\n",
			p->num_partition_entries);
		return -1;
	}

	for (int retry = 0; retry < 3; retry++) {
		uint64_t lba_offset = 0, byte_offset = 0;
		uint64_t entries_offset;

		switch (retry) {
		case 0:
			lba_offset = le64_to_cpu(p->hdr.partition_entry_lba);
			break;
		case 1:
			lba_offset = le64_to_cpu(p->hdr.partition_entry_lba)
				- le64_to_cpu(p->hdr.my_lba);
			byte_offset = filestart(fd) + gpt_offset;
			break;
		default:
			lba_offset = 1;
			byte_offset = filestart(fd) + gpt_offset;
			break;
		}

		entries_offset = lba2sz(lba_offset) + byte_offset;
		ret = gpt_load_entries(p, entries_offset, force_type, fd);
		if (ret == 0)
			break;
	}

	if (ret < 0)
		return ret;

	p->fd = fd;
	gpt_register_disk_partitions(p);

	return 0;
}

static char *string_lba_size(char *s, size_t bufsz, uint64_t lba)
{
	uint64_t bytes = lba2sz(lba);

	if (bytes < SIZE_KB(1))
		snprintf(s, bufsz, "%5" PRIu64, bytes);
	else if (bytes < SIZE_MB(1))
		snprintf(s, bufsz, "%4" PRIu64 "K", bytes >> 10);
	else if (bytes <= SIZE_GB(1))
		snprintf(s, bufsz, "%4" PRIu64 "M", bytes >> 20);
	else
		snprintf(s, bufsz, "%2" PRIu64 ".%" PRIu64 "G", bytes >> 30,
			 (bytes & SIZEMASK_GB) * 10 / SIZE_GB(1));

	return s;
}

struct fdisk_option_value {
	bool	l;
	bool	x;
	bool	n;
};

static const struct xopt_option fdisk_options[] = {
	{
		.name	= 'l',
		.type	= XOPT_TYPE_BOOL,
		.offset	= offsetof(struct fdisk_option_value, l),
	}, {
		.name	= 'x',
		.type	= XOPT_TYPE_BOOL,
		.offset	= offsetof(struct fdisk_option_value, x),
	}, {
		.name	= 'n',
		.type	= XOPT_TYPE_BOOL,
		.offset	= offsetof(struct fdisk_option_value, n),
	},
	LIBXOPT_NULLOPTION,
};

static int gpt_do_fdisk(struct gpt_editor_private_data *p, int argc, char **argv)
{
	struct fdisk_option_value arg = { 0 };
	char sz[32], guid[64];
	int ret;

	if (get_verbose_level() > 1) {
		structure_print("%-30s", &p->hdr, structure_gpt_header);
		putchar('\n');
	}

	if (argc > 0) { /* compatible mode */
		struct xopt *xopt = libxopt_new(fdisk_options, 0);

		ret = libxopt_parse(xopt, argc, argv, &arg);
		libxopt_free(xopt);

		if (ret < 0) {
			fprintf(stderr, "Error: %s\n", libxopt_get_error(xopt));
			return ret;
		}
	}

	snprintf_uuid(guid, sizeof(guid), 0, "%02X", &p->hdr.disk_guid);
	printf("Disklabel type: gpt\n");
	printf("disk identifier: %s\n", guid);

	if (arg.x) {
		printf("First LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.first_usable_lba));
		printf("Last LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.last_usable_lba));
		printf("Alternative LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.alternate_lba));
		printf("Partition entries LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.partition_entry_lba));
		printf("Allocated partition entries: %d\n", p->num_partition_entries);
	}

	printf("\n");

	printf("Device             Start      End Sectors");
	if (arg.x) {
		printf(" Type-UUID                            UUID                                 Name   ");
	} else {
		printf("  Size");
		if (arg.n)
			printf("  Name");
	}
	printf("\n");

	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];
		uint64_t starting_lba, ending_lba, sz_lba;
		uint8_t aligned_partname[sizeof(entry->partition_name)];
		char name[128];

		starting_lba = le64_to_cpu(entry->starting_lba);
		ending_lba = le64_to_cpu(entry->ending_lba);
		sz_lba = ending_lba - starting_lba + 1;

		if (starting_lba == 0 || ending_lba == 0)
			break;

		/* to avoid taking address of packed member of
		 * ‘struct gpt_entry’ may result in an
		 * unaligned pointer value
		 */
		memcpy(aligned_partname, entry->partition_name,
			sizeof(entry->partition_name));
		gpt_partition_name_to_char(
			(const __le16 *)aligned_partname,
			name, sizeof(name));

		printf("/dev/mmcblk0p%-2d %8" PRIu64 " %8" PRIu64 " %7" PRIu64 " ",
		       i + 1, starting_lba, ending_lba, sz_lba);

		if (arg.x) {
			snprintf_uuid(guid, sizeof(guid), 0, "%02X",
				      &entry->partition_type_guid);
			printf("%s ", guid);

			snprintf_uuid(guid, sizeof(guid), 0, "%02X",
				      &entry->unique_partition_guid);
			printf("%s ", guid);

			printf("%s", name);
		} else {
			string_lba_size(sz, sizeof(sz), sz_lba);
			printf("%s", sz);

			if (arg.n)
				printf("  %s", name);
		}

		printf("\n");
	}

	return 0;
}

static int gpt_show(struct gpt_editor_private_data *p, int argc, char **argv)
{
	structure_print("%-30s: ", &p->hdr, structure_gpt_header);

	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];

		if (le32_to_cpu(entry->starting_lba) == 0)
			break;
		if (le32_to_cpu(entry->ending_lba) == 0)
			break;

		printf("part %d:\n", i);
		structure_print("    %-30s: ", entry, structure_gpt_entry);
	}

	return 0;
}

/* print as "partitions" format defined in u-boot.
 * `gpt write mmc 0 $partitions` in u-boot will work.
 */
struct list_partitions_arg {
	bool		human;
	bool		c;
	bool		uenv;	/* u-boot env */
	bool		reverse;
};

static void list_partitions_print(const struct list_partitions_arg *arg,
				  FILE *fp, const char *s)
{
	char linebuf[1024];

	if (arg->uenv || arg->reverse)
		snprintf(linebuf, sizeof(linebuf), "%s", s);
	else if (arg->c)
		snprintf(linebuf, sizeof(linebuf), "\"%s\" \\\n", s);
	else /* default is human mode */
		snprintf(linebuf, sizeof(linebuf), "%s\n", s);

	fprintf(fp, "%s", linebuf);
}

static const struct xopt_option list_partitions_options[] = {
	{
		.long_name	= "human",
		.type		= XOPT_TYPE_BOOL,
		.offset		= offsetof(struct list_partitions_arg, human),
	}, {
		.name		= 'c',
		.type		= XOPT_TYPE_BOOL,
		.offset		= offsetof(struct list_partitions_arg, c),
	}, {
		.long_name	= "uenv",
		.type		= XOPT_TYPE_BOOL,
		.offset		= offsetof(struct list_partitions_arg, uenv),
	}, {
		.long_name	= "reverse",
		.type		= XOPT_TYPE_BOOL,
		.offset		= offsetof(struct list_partitions_arg, reverse),
	},
	LIBXOPT_NULLOPTION
};

static const char *partition_string_lba_size(char *s, size_t bufsz, uint64_t lba)
{
	uint64_t bytes = lba2sz(lba);

	if (bytes < SIZE_KB(1))
		snprintf(s, bufsz, "%" PRIu64, bytes);
	else if (bytes < SIZE_MB(1))
		snprintf(s, bufsz, "%" PRIu64 "K", bytes >> 10);
	else if (bytes <= SIZE_GB(1) || (bytes % SIZE_GB(1)))
		snprintf(s, bufsz, "%" PRIu64 "M", bytes >> 20);
	else
		snprintf(s, bufsz, "%" PRIu64 "G", bytes >> 30);

	return s;
}

static int to_partition_lba_size(const char *s, uint64_t *lba_size)
{
	uint64_t size;
	char *endp;

	size = strtoul(s, &endp, 10);
	switch (*endp) {
	case 'k':
	case 'K':
		size <<= 10;
		break;
	case 'm':
	case 'M':
		size <<= 20;
		break;
	case 'g':
	case 'G':
		size <<= 30;
		break;
	default:
		return -1;
	}

	*lba_size = size / LBA_BLKSZ;
	return 0;
}

static int gpt_list_partitions(struct gpt_editor_private_data *p,
			       int argc, char **argv)
{
	struct xopt *xopt = libxopt_new(list_partitions_options, 0);
	struct list_partitions_arg arg = { false };
	uint64_t lba = le64_to_cpu(p->hdr.first_usable_lba);
	struct efi_guid disk_type_uid;
	int same_disk_type_uid = 1;
	char desc[512], guid[128];
	int ret;

	ret = libxopt_parse(xopt, argc, argv, &arg);
	libxopt_free(xopt);
	if (ret < 0)
		return ret;

	/* test if all the partitions has the same type id */
	disk_type_uid = p->partitions[0].partition_type_guid;
	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];
		uint64_t ending_lba;

		ending_lba = le64_to_cpu(entry->ending_lba);
		if (ending_lba == 0)
			break;

		if (memcmp(&disk_type_uid, &entry->partition_type_guid,
		    sizeof(disk_type_uid))) {
			same_disk_type_uid = 0;
			break;
		}
	}

	if (arg.reverse) {
		printf("imgeditor --type gpt -- partitions \\\n");

		if (le64_to_cpu(p->hdr.first_usable_lba) != GPT_DEF_FIRST_LBA)
			printf("\t--first-lba %lu \\\n",
				(unsigned long)le64_to_cpu(p->hdr.first_usable_lba));

		printf("\t--last-lba %lu \\\n",
			(unsigned long)le64_to_cpu(p->hdr.last_usable_lba));
		printf("\t--alt-lba %lu \\\n",
			(unsigned long)le64_to_cpu(p->hdr.alternate_lba));

		if (le64_to_cpu(p->hdr.partition_entry_lba) != GPT_DEF_PART_ENTRY_LBA)
			printf("\t--part-entry-lba %lu \\\n",
				(unsigned long)le64_to_cpu(p->hdr.partition_entry_lba));

		if (le32_to_cpu(p->hdr.num_partition_entries) != GPT_DEF_NUM_PARTS)
			printf("\t--num-parts %d \\\n",
				le32_to_cpu(p->hdr.num_partition_entries));

		if (same_disk_type_uid) {
			snprintf_uuid(guid, sizeof(guid), 0, "%02x", &disk_type_uid);
			printf("\t--disk-type-uid %s \\\n", guid);
		}

		printf("\texample-gpt-table.bin \\\n");
		printf("\t\"");
	}

	if (arg.uenv || arg.reverse)
		printf("partitions=");

	snprintf_uuid(guid, sizeof(guid), 0, "%02x", &p->hdr.disk_guid);
	snprintf(desc, sizeof(desc), "uuid_disk=%s;", guid);
	list_partitions_print(&arg, stdout, desc);

	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];
		union gpt_entry_attributes attr;
		uint64_t starting_lba, ending_lba, sz_lba;
		uint8_t aligned_partname[sizeof(entry->partition_name)];
		char name[128], size[128];

		starting_lba = le64_to_cpu(entry->starting_lba);
		ending_lba = le64_to_cpu(entry->ending_lba);
		sz_lba = ending_lba - starting_lba + 1;

		if (starting_lba == 0 || ending_lba == 0)
			break;

		memcpy(aligned_partname, entry->partition_name,
		       sizeof(entry->partition_name));
		gpt_partition_name_to_char((const __le16 *)aligned_partname,
					   name, sizeof(name));

		snprintf(desc, sizeof(desc), "name=%s,", name);

		if (i == 0 || lba != starting_lba) {
			char start[64];

			lba = starting_lba;

			partition_string_lba_size(start, sizeof(start), starting_lba);
			strncatprintf(desc, sizeof(desc), "start=%s,", start);
		}

		partition_string_lba_size(size, sizeof(size), sz_lba);
		strncatprintf(desc, sizeof(desc), "size=%s,", size);

		attr.raw = le64_to_cpu(entry->attributes.raw);
		if (attr.fields.legacy_bios_bootable)
			strncatprintf(desc, sizeof(desc), "bootable,");

		snprintf_uuid(guid, sizeof(guid), 0, "%02x",
			      &entry->unique_partition_guid);
		strncatprintf(desc, sizeof(desc), "uuid=%s", guid);

		if (!same_disk_type_uid) {
			snprintf_uuid(guid, sizeof(guid), 0, "%02x",
				&entry->unique_partition_guid);
			strncatprintf(desc, sizeof(desc), ",type=%s", guid);
		}

		strncatprintf(desc, sizeof(desc), ";");
		list_partitions_print(&arg, stdout, desc);

		lba = ending_lba + 1;
	}

	if (arg.reverse)
		printf("\"");
	if (arg.uenv || arg.reverse)
		printf("\n");

	return 0;
}

/* Save partition data to file.
 * Usage peek part-name save-file
 */
static int gpt_peek_partition(struct gpt_editor_private_data *p, int fd,
			      int argc, char **argv)
{
	int64_t n, total_length = filelength(fd);
	const char *part_name, *save_file;
	off64_t start, size;
	struct gpt_entry *e;
	int fd_peek;

	if (argc != 3) {
		fprintf(stderr, "Usage: peek part-name save-file\n");
		return -1;
	}

	part_name = argv[1];
	save_file = argv[2];
	e = gpt_find_part(p, part_name);
	if (!e) {
		fprintf(stderr, "Error: part %s is not found\n", part_name);
		return -1;
	}

	n = le64_to_cpu(e->ending_lba);
	if (lba2sz(n) > total_length) {
		fprintf(stderr, "Error: ending lba is overrange\n");
		return -1;
	}

	fd_peek = fileopen(save_file, O_RDWR | O_CREAT, 0664);
	if (fd_peek < 0) {
		fprintf(stderr, "Error: create %s failed(%m)\n", save_file);
		return fd;
	}

	start = le64_to_cpu(e->starting_lba);
	start = lba2sz(start);

	size = le64_to_cpu(e->ending_lba) - le64_to_cpu(e->starting_lba) + 1;
	size = lba2sz(size);

	dd64(fd, fd_peek, start, 0, size, NULL, NULL);
	close(fd_peek);

	return 0;
}

static int gpt_list_main(void *private_data, int fd, int argc, char **argv)
{
	struct gpt_editor_private_data *p = private_data;

	if (argc == 0)
		return gpt_do_fdisk(p, argc, argv);

	if (!strcmp(argv[0], "show"))
		return gpt_show(p, argc, argv);
	else if (!strcmp(argv[0], "fdisk"))
		return gpt_do_fdisk(p, argc, argv);
	else if (!strcmp(argv[0], "partitions"))
		return gpt_list_partitions(p, argc, argv);
	else if (!strcmp(argv[0], "peek"))
		return gpt_peek_partition(p, fd, argc, argv);

	fprintf(stderr, "Error: unsupported command %s\n", argv[0]);
	return -1;
}

static cJSON *gpt_to_json(struct gpt_editor_private_data *p)
{
	uint64_t lba = le64_to_cpu(p->hdr.first_usable_lba);
	cJSON *root = cJSON_CreateObject();
	cJSON *parts;

	if (!root)
		return root;

	structure_save_json(root, &p->hdr, structure_gpt_header);

	parts = cJSON_AddArrayToObject(root, "partitions");
	if (!parts) {
		fprintf(stderr, "Error: create [partitions] failed\n");
		cJSON_Delete(root);
		return NULL;
	}

	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];
		uint64_t starting_lba, ending_lba, sz_lba;
		char s[64];
		cJSON *part;

		starting_lba = le64_to_cpu(entry->starting_lba);
		ending_lba = le64_to_cpu(entry->ending_lba);
		sz_lba = ending_lba - starting_lba + 1;

		if (starting_lba == 0 || ending_lba == 0)
			break;

		part = cJSON_CreateObject();
		if (!part)
			break;

		if (lba != starting_lba) {
			lba = starting_lba;

			snprintf(s, sizeof(s), "%" PRIu64, lba);
			json_create_or_update_string_value(part,
							   "starting_lba",
							   s);
		}

		snprintf(s, sizeof(s), "%" PRIu64, sz_lba);
		json_create_or_update_string_value(part, "lba_size", s);
		lba = ending_lba + 1;

		structure_save_json(part, &p->partitions[i],
				    structure_gpt_entry);
		cJSON_AddItemToArray(parts, part);
	}

	return root;
}

static int gpt_from_json(struct gpt_editor_private_data *p, cJSON *root)
{
	cJSON *parts = cJSON_GetObjectItem(root, "partitions");
	uint64_t lba;
	int ret = 0;

	if (!parts || !cJSON_IsArray(parts)) {
		fprintf(stderr, "Error: [partitions] is not found\n");
		return -1;
	}

	ret = structure_load_json(root, &p->hdr, structure_gpt_header);
	if (ret < 0)
		goto done;

	p->num_partition_entries = le32_to_cpu(p->hdr.num_partition_entries);
	p->partitions = calloc(p->num_partition_entries,
			       sizeof(*p->partitions));
	if (!p->partitions) {
		fprintf(stderr, "Error: alloc %d partitions failed\n",
			p->num_partition_entries);
		return -1;
	}

	if (cJSON_GetArraySize(parts) >
		(int)le32_to_cpu(p->hdr.num_partition_entries)) {
		fprintf(stderr, "Error: too many partitions. The maximum "
			"number is %d\n",
			le32_to_cpu(p->hdr.num_partition_entries));
		ret = -1;
		goto done;
	}

	lba = le64_to_cpu(p->hdr.first_usable_lba);
	if (lba <= le64_to_cpu(p->hdr.my_lba)) {
		fprintf(stderr, "Error: first usable lba is not valid\n");
		ret = -1;
		goto done;
	}

	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];
		cJSON *part = cJSON_GetArrayItem(parts, i);
		unsigned long start_lba, sz_lba = 0;
		const char *s;

		/* end of partitions defined in json */
		if (!part)
			break;

		ret = structure_load_json(part, entry, structure_gpt_entry);
		if (ret < 0)
			goto done;

		s = json_get_string_value_in_object(part, "starting_lba");
		if (s) {
			int err;

			start_lba = strict_strtoul(s, 10, &err, NULL);
			if (err) {
				fprintf(stderr, "Error: bad starting_lba %s\n",
					s);
				ret = -1;
				goto done;
			}
		} else {
			start_lba = lba;
		}

		s = json_get_string_value_in_object(part, "lba_size");
		if (!s) {
			fprintf(stderr, "Error: lba_size is not found\n");
			ret = -1;
			goto done;
		}

		sz_lba = strict_strtoul_or_default(s, 10, NULL, 0);
		if (!sz_lba) {
			fprintf(stderr, "Error: bad lba_size %s\n", s);
			ret = -1;
			goto done;
		}

		entry->starting_lba = cpu_to_le64(start_lba);
		entry->ending_lba = cpu_to_le64(start_lba + sz_lba - 1);
		lba = start_lba + sz_lba;
	}

done:
	if (ret < 0) {
		free(p->partitions);
		p->partitions = NULL;
	}

	return ret;
}

static int gpt_unpack(void *private_data, int fd, const char *out_filename,
		      int argc, char **argv)
{
	struct gpt_editor_private_data *p = private_data;
	cJSON *root = gpt_to_json(p);

	if (root) {
		json_saveto_file(root, out_filename);
		cJSON_Delete(root);
		return 0;
	}

	return -1;
}

static int gpt_write_mbr(int fd_outimg)
{
	const uint8_t magic[] = { 0x55, 0xaa };
	struct dos_partition mbr = {
		.sector		= 0x02,
		.sys_ind	= 0xee, /* for EFI */
		.end_head	= 0xff,
		.end_sector	= 0xff,
		.end_cyl	= 0xff,
		.start		= cpu_to_le32(1),
		.size		= cpu_to_le32(0xffffffff),
	};

	lseek(fd_outimg, DOS_PART_TBL_OFFSET, SEEK_SET);
	write(fd_outimg, &mbr, sizeof(mbr));

	lseek(fd_outimg, DOS_PART_MAGIC_OFFSET, SEEK_SET);
	write(fd_outimg, magic, sizeof(magic));

	return 0;
}

static int gpt_write(struct gpt_editor_private_data *p, int fd_outimg)
{
	size_t offset, partition_entries_sz;
	uint32_t num_partition_entries;

	if (!p->hdr.my_lba)
		p->hdr.my_lba = cpu_to_le64(1);
	if (!p->hdr.partition_entry_lba)
		p->hdr.partition_entry_lba = cpu_to_le64(GPT_DEF_PART_ENTRY_LBA);
	if (!p->hdr.sizeof_partition_entry)
		p->hdr.sizeof_partition_entry = cpu_to_le32(sizeof(struct gpt_entry));
	if (!p->hdr.num_partition_entries)
		p->hdr.num_partition_entries = cpu_to_le32(GPT_MAX_NUM_PARTS);

	if (le32_to_cpu(p->hdr.my_lba) > 0)
		gpt_write_mbr(fd_outimg);

	memcpy(&p->hdr.signature, gpt_signature, strlen(gpt_signature));
	p->hdr.revision = cpu_to_le32(GPT_HEADER_REVISION_V1);
	p->hdr.header_size = cpu_to_le32(sizeof(p->hdr));

	offset = lba2sz(cpu_to_le32(p->hdr.partition_entry_lba));
	num_partition_entries = le32_to_cpu(p->hdr.num_partition_entries);

	for (uint32_t i = 0; i < num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];

		lseek64(fd_outimg, offset, SEEK_SET);
		write(fd_outimg, entry, sizeof(*entry));

		offset += le32_to_cpu(p->hdr.sizeof_partition_entry);
	}

	partition_entries_sz = num_partition_entries *
			le32_to_cpu(p->hdr.sizeof_partition_entry);
	p->hdr.partition_entry_array_crc32 =
		cpu_to_le32(crc32(0, (uint8_t *)p->partitions,
			    partition_entries_sz));

	lseek64(fd_outimg, lba2sz(cpu_to_le32(p->hdr.my_lba)), SEEK_SET);
	p->hdr.header_crc32 = cpu_to_le32(gpt_header_calc_crc32(&p->hdr));

	/* gcc version 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04) in ci compile
	 * this file failed:
	 * error: ‘write’ reading 92 bytes from a region of size 8 [-Werror=stringop-overread]
	 * 1136 |         write(fd_outimg, &p->hdr, sizeof(p->hdr));
	 *
	 * I can not known the real causes but I known how to fix it.
	 */
	struct gpt_header tmp_header = p->hdr;

	write(fd_outimg, &tmp_header, sizeof(tmp_header));

	return 0;
}

static int gpt_pack(void *private_data, const char *cfg, int fd_outimg,
		    int argc, char **argv)
{
	struct gpt_editor_private_data *p = private_data;
	cJSON *root = json_from_file(cfg);
	int ret;

	if (!root)
		return -1;

	ret = gpt_from_json(p, root);
	cJSON_Delete(root);
	if (ret < 0)
		return ret;

	return gpt_write(p, fd_outimg);
}

/* usage:
 * `imgeditor --type gpt -- partitions mmc-part.bin ${partitions}`
 */
struct write_partitions_arg {
	unsigned long		flash_size;

	unsigned long		first_lba;
	unsigned long		last_lba;
	unsigned long		alt_lba;
	unsigned long		part_entry_lba;
	int			num_parts;

	const char		*disk_type_uid;

	const char		*out_filename;
	const char		*partitions;
};

static int write_partitions_arg_autofix(struct write_partitions_arg *arg)
{
	int arg_fixed = 0;

	if (arg->flash_size == 0)
		return arg_fixed;

	/* the fix logic is based on gpt_fill_header in u-boot */
	if (arg->last_lba == 0) {
		++arg_fixed;
		arg->last_lba = arg->flash_size - GPT_DEF_FIRST_LBA;
	}

	if (arg->alt_lba == 0) {
		++arg_fixed;
		arg->alt_lba = arg->flash_size - 1;
	}

	return arg_fixed;
}

static const struct xopt_option write_partitions_options[] = {
	{
		.long_name	= "flash-size",
		.type		= XOPT_TYPE_ULONG_DEC,
		.offset		= offsetof(struct write_partitions_arg, flash_size),
	}, {
		.long_name	= "first-lba",
		.type		= XOPT_TYPE_ULONG_DEC,
		.offset		= offsetof(struct write_partitions_arg, first_lba),
	}, {
		.long_name	= "last-lba",
		.type		= XOPT_TYPE_ULONG_DEC,
		.offset		= offsetof(struct write_partitions_arg, last_lba),
	}, {
		.long_name	= "alt-lba",
		.type		= XOPT_TYPE_ULONG_DEC,
		.offset		= offsetof(struct write_partitions_arg, alt_lba),
	}, {
		.long_name	= "part-entry-lba",
		.type		= XOPT_TYPE_ULONG_DEC,
		.offset		= offsetof(struct write_partitions_arg, part_entry_lba),
	}, {
		.long_name	= "num-parts",
		.type		= XOPT_TYPE_INT_DEC,
		.offset		= offsetof(struct write_partitions_arg, num_parts),
	}, {
		.long_name	= "disk-type-uid",
		.type		= XOPT_TYPE_STRING,
		.offset		= offsetof(struct write_partitions_arg, disk_type_uid),
	},
	LIBXOPT_NULLOPTION,
};

static void gpt_write_partitions_usage(void)
{
	fprintf(stderr, "Usage: imgeditor --type gpt -- partitions [OPTIONS] out.bin partitions=...\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  --flash-size lba          Set the total flash's size(LBA)\n");
	fprintf(stderr, "  --first-lba lba           Set gpt's first LBA\n");
	fprintf(stderr, "  --last-lba lba            Set gpt's last LBA\n");
	fprintf(stderr, "  --alt-lba lba             Set gpt's alternative LBA\n");
	fprintf(stderr, "  --part-entry-lba lba      Set gpt's partition_entry_lba\n");
	fprintf(stderr, "  --num-parts n             Set gpt's num_partition_entries\n");
	fprintf(stderr, "  --disk-type-uid uid       Set gpt partition's defualt disk type uid\n");
}

static bool expr_is_match(const char **ret_expr, const char *s)
{
	const char *expr = *ret_expr;
	size_t len = strlen(s);
	bool matched = false;

	if (!strncmp(expr, s, len)) {
		*ret_expr = expr + len;
		matched = true;
	}

	return matched;
}

static int gpt_parse_part(struct gpt_editor_private_data *p, const char *part)
{
	struct gpt_write_partition_pdata *wpdata = &p->wpdata;
	struct gpt_entry *entry = &p->partitions[wpdata->active_partition_entries];
	uint64_t lba_start = wpdata->next_lba, lba_size = 0;
	int is_diskpart = 1, with_type = 0;
	char *argv[32], buffer[1024];
	int argc;

	if (wpdata->active_partition_entries >= p->num_partition_entries) {
		fprintf(stderr, "Error: too much partitions\n");
		return -1;
	}

	snprintf(buffer, sizeof(buffer), "%s", part);
	argc = argv_buffer_split_with_delim(buffer, ",", argv, 32);

	for (int j = 0; j < argc; j++) {
		const char *expr = argv[j];
		int ret = 0;

		/* TODO: auto generate uid */
		if (expr_is_match(&expr, "uuid_disk=")) {
			is_diskpart = 0;
			ret = sscanf_uuid(&p->hdr.disk_guid, 0, expr);
		} else if (expr_is_match(&expr, "name=")) {
			__le16 partname[PARTNAME_SZ] = { 0 };

			set_gpt_partition_name(partname, expr);
			memcpy(&entry->partition_name, partname,
				sizeof(partname));

			wpdata->active_partition_entries++;
		} else if (expr_is_match(&expr, "start=")) {
			ret = to_partition_lba_size(expr, &lba_start);
		} else if (expr_is_match(&expr, "size=")) {
			ret = to_partition_lba_size(expr, &lba_size);
		} else if (expr_is_match(&expr, "uuid=")) {
			ret = sscanf_uuid(&entry->unique_partition_guid, 0, expr);
		} else if (expr_is_match(&expr, "type=")) {
			ret = sscanf_uuid(&entry->partition_type_guid, 0, expr);
		} else if (expr_is_match(&expr, "bootable")) {
			union gpt_entry_attributes attr = { .raw = 0 };

			attr.fields.legacy_bios_bootable = 1;
			entry->attributes.raw = cpu_to_le64(attr.raw);
		} else {
			fprintf(stderr, "Error: unsupported expr %s\n", expr);
			ret = -1;
		}

		if (ret < 0)
			return ret;
	}

	if (is_diskpart) {
		if (lba_size == 0) {
			fprintf(stderr, "Error: no size defined\n");
			return -1;
		}

		if (!with_type) {
			memcpy(&entry->partition_type_guid,
			       &wpdata->disk_type_uid,
			       sizeof(wpdata->disk_type_uid));
		}

		entry->starting_lba = cpu_to_le64(lba_start);
		entry->ending_lba = cpu_to_le64(lba_start + lba_size - 1);
		wpdata->next_lba = lba_start + lba_size;
	}

	return 0;
}

static int gpt_parse_partitions(struct gpt_editor_private_data *p,
				const char *partitions)
{
	char buffer[4096], *parts_argv[128];
	int parts_argc;

	if (!strncmp(partitions, "partitions=", 11))
		partitions += 11;
	snprintf(buffer, sizeof(buffer), "%s", partitions);

	parts_argc = argv_buffer_split_with_delim(buffer, ";", parts_argv, 128);
	for (int i = 0; i < parts_argc; i++) {
		int ret = gpt_parse_part(p, parts_argv[i]);

		if (ret < 0) {
			fprintf(stderr, "Error: parse [%s] failed\n",
				parts_argv[i]);
			return ret;
		}
	}

	return 0;
}

static int gpt_write_partitions(struct gpt_editor_private_data *p, int argc,
				char **argv)
{
	struct xopt *xopt = libxopt_new(write_partitions_options, 0);
	struct write_partitions_arg arg = {
		.first_lba = GPT_DEF_FIRST_LBA,
		.num_parts = GPT_DEF_NUM_PARTS,
		.part_entry_lba = GPT_DEF_PART_ENTRY_LBA,
	};
	int fd, optind, ret = -1;

	if (argc == 1) {
		gpt_write_partitions_usage();
		goto done;
	}

	ret = libxopt_parse(xopt, argc, argv, &arg);
	if (ret < 0) {
		gpt_write_partitions_usage();
		goto done;
	}

	optind = libxopt_get_optind(xopt);
	if (argc - optind < 2) {
		ret = -1;
		gpt_write_partitions_usage();
		goto done;
	}

	if (arg.flash_size == 0 && (arg.last_lba == 0 || arg.alt_lba == 0)) {
		fprintf(stderr, "Error: --flash-size is not set, "
				"can not detect");
		if (arg.last_lba == 0)
			fprintf(stderr, " --last-lba");
		if (arg.alt_lba == 0)
			fprintf(stderr, " --alt-lba");
		fprintf(stderr, "\n");

		ret = -1;
		goto done;
	}

	if (arg.disk_type_uid) {
		ret = sscanf_uuid(&p->wpdata.disk_type_uid, 0, arg.disk_type_uid);
		if (ret < 0) {
			fprintf(stderr, "Error: invaid --disk-type-uid %s\n",
				arg.disk_type_uid);
			goto done;
		}
	}

	if (arg.num_parts > GPT_MAX_NUM_PARTS) {
		fprintf(stderr, "Error: --num-parts %d is too large\n",
			arg.num_parts);
		ret = -1;
		goto done;
	}

	if (write_partitions_arg_autofix(&arg)) {
		printf("Auto generated param based on --flash-size %lu: "
			"--first-lba %lu --last-lba %lu --alt-lba %lu\n",
			arg.flash_size,
			arg.first_lba,
			arg.last_lba,
			arg.alt_lba);
	}

	arg.out_filename = argv[optind];
	arg.partitions = argv[optind + 1];

	fd = fileopen(arg.out_filename, O_RDWR | O_CREAT | O_TRUNC, 0664);
	if (fd < 0) {
		ret = fd;
		goto done;
	}

	p->wpdata.active_partition_entries = 0;
	p->wpdata.next_lba = arg.first_lba;

	p->num_partition_entries = GPT_MAX_NUM_PARTS;
	p->partitions = calloc(p->num_partition_entries, sizeof(*p->partitions));
	if (!p->partitions) {
		fprintf(stderr, "Error: alloc gpt partitions failed\n");
		ret = -1;
		goto done;
	}

	p->hdr.partition_entry_lba = cpu_to_le64(arg.part_entry_lba);
	p->hdr.num_partition_entries = cpu_to_le32(arg.num_parts);
	p->hdr.first_usable_lba = cpu_to_le64(arg.first_lba);
	p->hdr.last_usable_lba = cpu_to_le64(arg.last_lba);
	p->hdr.alternate_lba = cpu_to_le64(arg.alt_lba);

	ret = gpt_parse_partitions(p, arg.partitions);
	if (ret < 0)
		goto done;

	ret = gpt_write(p, fd);
	close(fd);

done:
	libxopt_free(xopt);
	return ret;
}

static int gpt_main(void *private_data, int argc, char **argv)
{
	struct gpt_editor_private_data *p = private_data;

	if (!strcmp(argv[0], "partitions"))
		return gpt_write_partitions(p, argc, argv);

	fprintf(stderr, "Error: unsupported subcommand %s\n", argv[0]);
	return -1;
}

static struct imgeditor gpt_editor = {
	.name			= "gpt",
	.descriptor		= "EFI GPT editor",
	.flags			= IMGEDITOR_FLAG_SINGLE_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= lba2sz(GPT_PRIMARY_PARTITION_TABLE_LBA) + sizeof(struct gpt_header),
	.private_data_size	= sizeof(struct gpt_editor_private_data),
	.detect			= gpt_detect,
	.list			= gpt_list_main,
	.unpack			= gpt_unpack,
	.pack			= gpt_pack,
	.main			= gpt_main,
	.exit			= gpt_exit,

	.search_magic		= {
		.magic		= gpt_signature,
		.magic_sz	= sizeof(gpt_signature) - 1,
		.magic_offset	= offsetof(struct gpt_header, signature),
	}
};
REGISTER_IMGEDITOR(gpt_editor);
