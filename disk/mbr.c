/*
 * MBR: Master Boot Record
 * qianfan Zhao
 */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include "imgeditor.h"
#include "structure.h"
#include "libxopt.h"

#define SECTOR_SIZE			512

static char *string_lba_size(char *s, size_t bufsz, uint64_t lba)
{
	uint64_t bytes = SECTOR_SIZE * lba;

	if (bytes < SIZE_KB(1))
		snprintf(s, bufsz, "%" PRIu64, bytes);
	else if (bytes < SIZE_MB(1))
		snprintf(s, bufsz, "%" PRIu64 "K", bytes >> 10);
	else if (bytes < SIZE_GB(1))
		snprintf(s, bufsz, "%" PRIu64 "M", bytes >> 20);
	else if (bytes & SIZEMASK_GB)
		snprintf(s, bufsz, "%" PRIu64 ".%" PRIu64 "G", bytes >> 30,
			(bytes & SIZEMASK_GB) * 10 / SIZE_GB(1));
	else
		snprintf(s, bufsz, "%" PRIu64 "G", bytes >> 30);

	return s;
}

#define DOS_PART_DISKSIG_OFFSET		0x1b8
#define DOS_PART_TBL_OFFSET		0x1be
#define DOS_PART_MAGIC_OFFSET		0x1fe
#define DOS_PBR_FSTYPE_OFFSET		0x36
#define DOS_PBR32_FSTYPE_OFFSET		0x52
#define DOS_PBR_MEDIA_TYPE_OFFSET	0x15
#define DOS_MBR				0
#define DOS_PBR				1

#define DOS_PART_TYPE_EXTENDED		0x05
#define DOS_PART_TYPE_FAT16		0x06
#define DOS_PART_TYPE_FAT32		0x0b
#define DOS_PART_TYPE_EFI		0xee
#define DOS_PART_TYPE_EXTENDED_LBA	0x0F
#define DOS_PART_TYPE_EXTENDED_LINUX	0x85

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

static inline int is_extended(int part_type)
{
	return (part_type == DOS_PART_TYPE_EXTENDED ||
		part_type == DOS_PART_TYPE_EXTENDED_LBA ||
		part_type == DOS_PART_TYPE_EXTENDED_LINUX);
}

static void dos_partition_chs(struct dos_partition *part, uint16_t *cyl,
			      uint8_t *head, uint8_t *sector)
{
	*head = part->head;
	*sector = part->sector & 0x3f;
	*cyl = ((part->sector >> 6) << 8) | part->cyl;
}

static void dos_partition_end_chs(struct dos_partition *part, uint16_t *cyl,
				  uint8_t *head, uint8_t *sector)
{
	*head = part->end_head;
	*sector = part->end_sector & 0x3f;
	*cyl = ((part->end_sector >> 6) << 8) | part->end_cyl;
}

static char *string_dos_partition_sys_ind(char *s, size_t sz, uint8_t sysind)
{
	const char *type = "?";

	switch (sysind) {
	case DOS_PART_TYPE_FAT16:
		type = "FAT16";
		break;
	case DOS_PART_TYPE_FAT32:
		type = "FAT32";
		break;
	case DOS_PART_TYPE_EXTENDED:
		type = "Extended";
		break;
	case DOS_PART_TYPE_EFI:
		type = "EFI";
		break;
	case 0x83:
		type = "Linux";
		break;
	}

	snprintf(s, sz, "%s", type);
	return s;
}

static void structure_dospart_print_sys_ind(const char *print_name_fmt,
					   const char *name,
					   const void *addr, size_t sz)
{
	uint8_t sys_ind = *(const uint8_t *)addr;
	char s[32];

	structure_print_name(print_name_fmt, name);
	string_dos_partition_sys_ind(s, sizeof(s), sys_ind);
	printf("0x%02x (%s)\n", sys_ind, s);
}

static void structure_dospart_print_size(const char *print_name_fmt,
					 const char *name,
					 const void *addr, size_t sz)
{
	uint32_t size = le32_to_cpu(*(const __le32 *)addr);
	char s[32];

	structure_print_name(print_name_fmt, name);
	string_lba_size(s, sizeof(s), size);
	printf("%d (%s)\n", size, s);
}

static const struct structure_item structure_dos_partition[] = {
	STRUCTURE_ITEM(struct dos_partition, boot_ind,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, head,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, sector,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, cyl,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, sys_ind,		structure_dospart_print_sys_ind),
	STRUCTURE_ITEM(struct dos_partition, end_head,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, end_sector,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, end_cyl,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, start,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct dos_partition, size,		structure_dospart_print_size),
	STRUCTURE_ITEM_END(),
};

#define DOS_PRIMARY_PARTITIONS		4
#define MAX_EXT_PARTS			256

struct mbr_editor_private_data {
	int			fd;
	uint32_t		disksig;
	struct dos_partition	primary[DOS_PRIMARY_PARTITIONS];

	size_t			logic_counts;
	struct dos_partition	logic[MAX_EXT_PARTS];
};

static int mbr_foreach_extended(int force_type, int fd, uint32_t extend_start,
				uint32_t part_start, void *data,
				int (*cb)(struct dos_partition *part,
					  uint32_t extend_start,
					  uint32_t part_start,
					  void *data))
{
	struct dos_partition parts[DOS_PRIMARY_PARTITIONS] = { 0 };
	uint32_t start = extend_start + part_start;
	uint8_t buf[SECTOR_SIZE];
	int ret;

	fileseek(fd, start * SECTOR_SIZE);
	ret = fileread(fd, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (buf[DOS_PART_MAGIC_OFFSET] != 0x55
	    && buf[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) {
		fprintf_if_force_type("Error: mbr(%d) magic doesn't match\n",
			start);
		return -1;
	}

	memcpy(parts, &buf[DOS_PART_TBL_OFFSET], sizeof(parts));
	for (int i = 0; i < DOS_PRIMARY_PARTITIONS; i++) {
		struct dos_partition *part = &parts[i];

		if (part->sys_ind == 0)
			continue;

		ret = cb(part, extend_start, part_start, data);
		if (ret < 0)
			return ret;

		if (is_extended(part->sys_ind)) {
			return mbr_foreach_extended(force_type, fd,
						    extend_start,
						    le32_to_cpu(part->start),
						    data, cb);
		}
	}

	return 0;
}

static int mbr_save_logic_cb(struct dos_partition *part, uint32_t extend_start,
			     uint32_t part_start, void *data)
{
	struct mbr_editor_private_data *p = data;
	struct dos_partition *logic = &p->logic[p->logic_counts];
	uint32_t logic_start;

	if (p->logic_counts > MAX_EXT_PARTS) {
		fprintf(stderr, "Error: too many logic partitions\n");
		return -1;
	}

	if (is_extended(part->sys_ind))
		return 0;

	memcpy(logic, part, sizeof(*part));
	logic_start = le32_to_cpu(part->start) + part_start + extend_start;
	logic->start = cpu_to_le32(logic_start);
	p->logic_counts++;

	return 0;
}

static int mbr_detect(void *private_data, int force_type, int fd)
{
	struct mbr_editor_private_data *p = private_data;
	uint8_t buf[SECTOR_SIZE];
	int ret;

	p->fd = fd;

	ret = read(fd, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (buf[DOS_PART_MAGIC_OFFSET] != 0x55
	    && buf[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	/* disksig is a le32 number */
	p->disksig = (buf[DOS_PART_DISKSIG_OFFSET + 0] << 0)
			| (buf[DOS_PART_DISKSIG_OFFSET + 1] << 8)
			| (buf[DOS_PART_DISKSIG_OFFSET + 2] << 16)
			| (buf[DOS_PART_DISKSIG_OFFSET + 3] << 24);

	/* copy the whole partition entrys */
	memcpy(p->primary, &buf[DOS_PART_TBL_OFFSET], sizeof(p->primary));

	/* EFI is compatible MBR */
	if (p->primary[0].sys_ind == DOS_PART_TYPE_EFI && !force_type) {
		/* let the gpt.c handle it */
		return -1;
	}

	for (int i = 0; i < DOS_PRIMARY_PARTITIONS; i++) {
		struct dos_partition *primary = &p->primary[i];

		/* only one extended partition */
		if (is_extended(primary->sys_ind)) {
			uint32_t extend_start = le32_to_cpu(primary->start);

			return mbr_foreach_extended(force_type, fd,
						    extend_start, 0,
						    p,
						    mbr_save_logic_cb);
		}
	}

	return 0;
}

struct mbr_print_logic_arg {
	int	part_idx;
};

static int mbr_print_logic_cb(struct dos_partition *part, uint32_t extend_start,
			      uint32_t part_start, void *data)
{
	struct mbr_print_logic_arg *arg = data;

	printf("Partition %d:\n", arg->part_idx);
	structure_print("    %-20s: ", part, structure_dos_partition);
	printf("\n");

	arg->part_idx++;
	return 0;
}

static int mbr_do_parts(struct mbr_editor_private_data *p, int argc, char **argv)
{
	struct mbr_print_logic_arg arg = { .part_idx = DOS_PRIMARY_PARTITIONS + 1};
	uint32_t has_extend = 0, extend_start = 0;

	for (int i = 0; i < DOS_PRIMARY_PARTITIONS; i++) {
		struct dos_partition *part = &p->primary[i];

		if (!has_extend && is_extended(part->sys_ind)) {
			extend_start = le32_to_cpu(part->start);
			has_extend = 1;
		}

		printf("Partition %d:\n", i + 1);
		structure_print("    %-20s: ", part, structure_dos_partition);
		printf("\n");
	}

	if (has_extend)
		return mbr_foreach_extended(1, p->fd, extend_start, 0,
					    &arg, mbr_print_logic_cb);

	return 0;
}

struct fdisk_option_value {
	bool	l;
	bool	x;
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
	},
	LIBXOPT_NULLOPTION,
};

static void mbr_do_fdisk_partition(struct dos_partition *part, int part_idx,
				   struct fdisk_option_value *arg)
{
	uint32_t start, size;
	char tmp[64];

	start = le32_to_cpu(part->start);
	size = le32_to_cpu(part->size);
	if (size == 0)
		return;

	printf("/dev/sda%-2d ", part_idx);
	printf("%s ", part->boot_ind & 0x80 ? "boot" : "    ");
	printf("%8d %8d %8d ", start, start + size - 1, size);

	if (!arg->x) {
		string_lba_size(tmp, sizeof(tmp), size);
		printf("%5s ", tmp);
	}

	string_dos_partition_sys_ind(tmp, sizeof(tmp), part->sys_ind);
	printf("%2x %-8s ", part->sys_ind, tmp);

	if (arg->x) {
		uint8_t head, sector;
		uint16_t cyl;

		dos_partition_chs(part, &cyl, &head, &sector);
		snprintf(tmp, sizeof(tmp), "%d/%d/%d", cyl, head, sector);
		printf("%11s ", tmp);

		dos_partition_end_chs(part, &cyl, &head, &sector);
		snprintf(tmp, sizeof(tmp), "%d/%d/%d", cyl, head, sector);
		printf("%10s ", tmp);
	}

	printf("\n");
}

static int mbr_do_fdisk(struct mbr_editor_private_data *p, int argc, char **argv)
{
	struct xopt *xopt = libxopt_new(fdisk_options, LIBXOPT_FLAG_KEEPFIRST);
	struct fdisk_option_value arg = { 0 };
	int part_idx = 1;
	int ret;

	ret = libxopt_parse(xopt, argc, argv, &arg);
	if (ret < 0) {
		fprintf(stderr, "Error: %s\n", libxopt_get_error(xopt));
		libxopt_free(xopt);
		return ret;
	}

	libxopt_free(xopt);

	printf("Disklabel type: dos\n");
	printf("disk identifier: 0x%08x\n", p->disksig);
	printf("\n");

	printf("Device     Boot    Start      End  Sectors");
	if (!arg.x)
		printf("  Size");
	printf(" Id Type");

	if (arg.x)
		printf("     Start-C/H/S  End-C/H/S");
	printf("\n");

	for (int i = 0; i < DOS_PRIMARY_PARTITIONS; i++)
		mbr_do_fdisk_partition(&p->primary[i], part_idx++, &arg);

	for (size_t i = 0; i < p->logic_counts; i++)
		mbr_do_fdisk_partition(&p->logic[i], part_idx++, &arg);

	return 0;
}

static int mbr_main(void *private_data, int fd, int argc, char **argv)
{
	struct mbr_editor_private_data *p = private_data;

	if (argc >= 1) {
		if (!strcmp(argv[0], "parts"))
			return mbr_do_parts(p, argc, argv);
	}

	/* default print in fdisk format */
	return mbr_do_fdisk(p, argc, argv);
}

static struct imgeditor mbr_editor = {
	.name			= "mbr",
	.descriptor		= "dos mbr editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= SECTOR_SIZE,
	.private_data_size	= sizeof(struct mbr_editor_private_data),
	.detect			= mbr_detect,
	.list			= mbr_main,
};
REGISTER_IMGEDITOR(mbr_editor);
