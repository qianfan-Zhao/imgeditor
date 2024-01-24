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

static void printf_wchar_string(const __le16 *ws, size_t maxsz)
{
	for (size_t i = 0; i < maxsz / sizeof(__le16); i++) {
		int c = le16_to_cpu(ws[i]);

		if (c == '\0')
			break;
		putchar(c);
	}
}

static void structure_item_print_wchar_string(const char *print_name_fmt,
					      const char *name,
					      const void *addr,
					      size_t sz)
{
	structure_print_name(print_name_fmt, name);
	printf_wchar_string(addr, sz);
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

	for (size_t i = 0; i < sz / sizeof(__le16) && s[i]; i++)
		ws[i] = cpu_to_le16(s[i]);

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

struct gpt_editor_private_data {
	int			fd;
	struct gpt_header	hdr;

	struct gpt_entry	*partitions;
	uint32_t		num_partition_entries;
};

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
	return 0;
}

static int gpt_do_hdr(struct gpt_editor_private_data *p, int argc, char **argv)
{
	structure_print("%-30s: ", &p->hdr, structure_gpt_header);
	return 0;
}

static int gpt_do_parts(struct gpt_editor_private_data *p, int argc, char **argv)
{
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

static int gpt_do_fdisk(struct gpt_editor_private_data *p, int argc, char **argv)
{
	#define FDISK_FLAG_l		(1 << 0)
	#define FDISK_FLAG_x		(1 << 1)
	unsigned int fdisk_flags = 0;
	char sz[32], guid[64];

	if (get_verbose_level() > 1) {
		structure_print("%-30s", &p->hdr, structure_gpt_header);
		putchar('\n');
	}

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-l"))
			fdisk_flags |= FDISK_FLAG_l;
		else if (!strcmp(argv[i], "-x"))
			fdisk_flags |= FDISK_FLAG_x;
		else if (!strcmp(argv[i], "-lx"))
			fdisk_flags |= (FDISK_FLAG_l | FDISK_FLAG_x);
	}

	snprintf_uuid(guid, sizeof(guid), 0, "%02X", &p->hdr.disk_guid);
	printf("Disklabel type: gpt\n");
	printf("disk identifier: %s\n", guid);

	if (fdisk_flags & FDISK_FLAG_x) {
		printf("First LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.first_usable_lba));
		printf("Last LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.last_usable_lba));
		printf("Alternative LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.alternate_lba));
		printf("Partition entries LBA: %" PRIu64 "\n", le64_to_cpu(p->hdr.partition_entry_lba));
		printf("Allocated partition entries: %d\n", p->num_partition_entries);
	}

	printf("\n");

	printf("Device             Start      End Sectors");
	if (fdisk_flags & FDISK_FLAG_x)
		printf(" Type-UUID                            UUID                                 Name   ");
	else
		printf("  Size");
	printf("\n");

	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];
		uint64_t starting_lba, ending_lba, sz_lba;

		starting_lba = le64_to_cpu(entry->starting_lba);
		ending_lba = le64_to_cpu(entry->ending_lba);
		sz_lba = ending_lba - starting_lba + 1;

		if (starting_lba == 0 || ending_lba == 0)
			break;

		printf("/dev/mmcblk0p%-2d %8" PRIu64 " %8" PRIu64 " %7" PRIu64 " ",
		       i + 1, starting_lba, ending_lba, sz_lba);

		if (fdisk_flags & FDISK_FLAG_x) {
			snprintf_uuid(guid, sizeof(guid), 0, "%02X",
				      &entry->partition_type_guid);
			printf("%s ", guid);

			snprintf_uuid(guid, sizeof(guid), 0, "%02X",
				      &entry->unique_partition_guid);
			printf("%s ", guid);

			printf_wchar_string(entry->partition_name,
					    sizeof(entry->partition_name));
		} else {
			string_lba_size(sz, sizeof(sz), sz_lba);
			printf("%s", sz);
		}

		printf("\n");
	}

	return 0;
}

static int gpt_main(void *private_data, int fd, int argc, char **argv)
{
	struct gpt_editor_private_data *p = private_data;

	if (argc >= 1) {
		if (!strcmp(argv[0], "hdr"))
			return gpt_do_hdr(p, argc, argv);
		else if (!strcmp(argv[0], "parts"))
			return gpt_do_parts(p, argc, argv);
	}

	/* default print in fdisk format */
	return gpt_do_fdisk(p, argc, argv);
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

	if (le32_to_cpu(p->hdr.my_lba) > 0)
		gpt_write_mbr(fd_outimg);

	memcpy(&p->hdr.signature, gpt_signature, strlen(gpt_signature));
	p->hdr.revision = cpu_to_le32(GPT_HEADER_REVISION_V1);
	p->hdr.header_size = cpu_to_le32(sizeof(p->hdr));

	offset = lba2sz(cpu_to_le32(p->hdr.partition_entry_lba));
	for (uint32_t i = 0; i < p->num_partition_entries; i++) {
		struct gpt_entry *entry = &p->partitions[i];

		lseek64(fd_outimg, offset, SEEK_SET);
		write(fd_outimg, entry, sizeof(*entry));

		offset += le32_to_cpu(p->hdr.sizeof_partition_entry);
	}

	partition_entries_sz = p->num_partition_entries *
			le32_to_cpu(p->hdr.sizeof_partition_entry);
	p->hdr.partition_entry_array_crc32 =
		cpu_to_le32(crc32(0, (uint8_t *)p->partitions,
			    partition_entries_sz));

	lseek64(fd_outimg, lba2sz(cpu_to_le32(p->hdr.my_lba)), SEEK_SET);
	p->hdr.header_crc32 = cpu_to_le32(gpt_header_calc_crc32(&p->hdr));
	write(fd_outimg, &p->hdr, sizeof(p->hdr));

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

static struct imgeditor gpt_editor = {
	.name			= "gpt",
	.descriptor		= "EFI GPT editor",
	.flags			= IMGEDITOR_FLAG_SINGLE_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= lba2sz(GPT_PRIMARY_PARTITION_TABLE_LBA) + sizeof(struct gpt_header),
	.private_data_size	= sizeof(struct gpt_editor_private_data),
	.detect			= gpt_detect,
	.list			= gpt_main,
	.unpack			= gpt_unpack,
	.pack			= gpt_pack,

	.search_magic		= {
		.magic		= gpt_signature,
		.magic_sz	= sizeof(gpt_signature) - 1,
		.magic_offset	= offsetof(struct gpt_header, signature),
	}
};
REGISTER_IMGEDITOR(gpt_editor);
