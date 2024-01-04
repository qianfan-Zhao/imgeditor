/*
 * EFI GUID Partition Table
 * qianfan Zhao
 */
#define _LARGEFILE64_SOURCE
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include "imgeditor.h"
#include "structure.h"
#include "string_helper.h"

#define LBA_BLKSZ				512
#define lba2sz(lba)				((lba) * LBA_BLKSZ)

#define GPT_PRIMARY_PARTITION_TABLE_LBA		1

struct efi_guid {
	__u8 b[16];
};

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

static char *snprintf_uuid(char *buf, size_t sz, int isuuid, const char *char_fmt,
			   const struct efi_guid *guid)
{
	const uint8_t *order;
	char *p = buf;

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
	const uint8_t uuid_char_order[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8,
		9, 10, 11, 12, 13, 14, 15
	};

	const uint8_t guid_char_order[] = {
		3, 2, 1, 0, 5, 4, 7, 6,
		8, 9, 10, 11, 12, 13, 14, 15
	};

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

static void structure_item_print_guid(const char *print_name_fmt, const char *name,
				      const void *addr, size_t sz)
{
	char guid[64];

	structure_print_name(print_name_fmt, name);
	snprintf_uuid(guid, sizeof(guid), 0, "%02x", addr);
	printf("%s\n", guid);
}

static const struct structure_item structure_gpt_header[] = {
	STRUCTURE_ITEM(struct gpt_header, signature,			structure_item_print_string),
	STRUCTURE_ITEM(struct gpt_header, revision,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct gpt_header, header_size,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, header_crc32,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct gpt_header, my_lba,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, alternate_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, first_usable_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, last_usable_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, disk_guid,			structure_item_print_guid),
	STRUCTURE_ITEM(struct gpt_header, partition_entry_lba,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, num_partition_entries,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, sizeof_partition_entry,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_header, partition_entry_array_crc32,	structure_item_print_xunsigned),
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

static const struct structure_item structure_gpt_entry[] = {
	STRUCTURE_ITEM(struct gpt_entry, partition_type_guid,		structure_item_print_guid),
	STRUCTURE_ITEM(struct gpt_entry, unique_partition_guid,		structure_item_print_guid),
	STRUCTURE_ITEM(struct gpt_entry, starting_lba,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_entry, ending_lba,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct gpt_entry, attributes,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct gpt_entry, partition_name,		structure_item_print_wchar_string),
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

static int gpt_detect(void *private_data, int force_type, int fd)
{
	struct gpt_editor_private_data *p = private_data;
	size_t partition_entries_sz;
	uint32_t c32;
	int ret;

	static_assert(sizeof(struct gpt_header) == 92, "struct gpt_header");

	lseek64(fd, lba2sz(GPT_PRIMARY_PARTITION_TABLE_LBA), SEEK_SET);
	ret = read(fd, &p->hdr, sizeof(p->hdr));
	if (ret < 0)
		return ret;

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

	lseek64(fd, lba2sz(le32_to_cpu(p->hdr.partition_entry_lba)), SEEK_SET);
	partition_entries_sz = le32_to_cpu(p->hdr.sizeof_partition_entry);
	partition_entries_sz *= p->num_partition_entries;
	ret = read(fd, p->partitions, partition_entries_sz);
	if (ret < 0)
		return ret;

	if ((size_t)ret != partition_entries_sz) {
		fprintf_if_force_type("Error: read whole partitons failed\n");
		return -1;
	}

	c32 = crc32(0, (uint8_t *)p->partitions, partition_entries_sz);
	if (c32 != le32_to_cpu(p->hdr.partition_entry_array_crc32)) {
		fprintf_if_force_type("Error: partition_entry_array_crc32 doesn't match"
				      "(%08x != %08x)\n",
				      c32, p->hdr.partition_entry_array_crc32);
		return -1;
	}

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

static struct imgeditor gpt_editor = {
	.name			= "gpt",
	.descriptor		= "EFI GPT editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= lba2sz(GPT_PRIMARY_PARTITION_TABLE_LBA) + sizeof(struct gpt_header),
	.private_data_size	= sizeof(struct gpt_editor_private_data),
	.detect			= gpt_detect,
	.list			= gpt_main,
};
REGISTER_IMGEDITOR(gpt_editor);
