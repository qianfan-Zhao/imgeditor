/*
 * MBR: Master Boot Record
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

#define SECTOR_SIZE			512

#define DOS_PART_DISKSIG_OFFSET		0x1b8
#define DOS_PART_TBL_OFFSET		0x1be
#define DOS_PART_MAGIC_OFFSET		0x1fe
#define DOS_PBR_FSTYPE_OFFSET		0x36
#define DOS_PBR32_FSTYPE_OFFSET		0x52
#define DOS_PBR_MEDIA_TYPE_OFFSET	0x15
#define DOS_MBR				0
#define DOS_PBR				1
#define DOS_PART_TYPE_EXTENDED		0x05
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

static const struct structure_item structure_dos_partition[] = {
	STRUCTURE_ITEM(struct dos_partition, boot_ind,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, head,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, sector,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, cyl,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, sys_ind,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, end_head,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, end_sector,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, end_cyl,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct dos_partition, start,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct dos_partition, size,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

/* this tool only support the first 4 partition entry now,
 * and no plan to add logical partition yet.
 */
#define DOS_PRIMARY_PARTITIONS		4

struct mbr_editor_private_data {
	int			fd;
	uint32_t		disksig;
	struct dos_partition	primary[DOS_PRIMARY_PARTITIONS];
};

static int mbr_detect(void *private_data, int force_type, int fd)
{
	struct mbr_editor_private_data *p = private_data;
	uint8_t buf[SECTOR_SIZE];
	int ret;

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

	p->fd = fd;
	return 0;
}

static int mbr_do_parts(struct mbr_editor_private_data *p, int argc, char **argv)
{
	for (int i = 0; i < DOS_PRIMARY_PARTITIONS; i++) {
		struct dos_partition *part = &p->primary[i];

		printf("Partition %d:\n", i);
		structure_print("    %-20s: ", part, structure_dos_partition);
		printf("\n");
	}

	return 0;
}

static char *string_lba_size(char *s, size_t bufsz, uint32_t lba)
{
	uint64_t bytes = SECTOR_SIZE * lba;

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

static int mbr_do_fdisk(struct mbr_editor_private_data *p, int argc, char **argv)
{
	#define FDISK_FLAG_l		(1 << 0)
	#define FDISK_FLAG_x		(1 << 1)
	unsigned int fdisk_flags = 0;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-l"))
			fdisk_flags |= FDISK_FLAG_l;
		else if (!strcmp(argv[i], "-x"))
			fdisk_flags |= FDISK_FLAG_x;
		else if (!strcmp(argv[i], "-lx"))
			fdisk_flags |= (FDISK_FLAG_l | FDISK_FLAG_x);
	}

	printf("Disklabel type: dos\n");
	printf("disk identifier: 0x%08x\n", p->disksig);
	printf("\n");

	printf("Device     Boot    Start      End  Sectors");
	if (!(fdisk_flags & FDISK_FLAG_x))
		printf("  Size");
	printf(" Id Type");

	if (fdisk_flags & FDISK_FLAG_x)
		printf("     Start-C/H/S  End-C/H/S");
	printf("\n");

	for (int i = 0; i < DOS_PRIMARY_PARTITIONS; i++) {
		struct dos_partition *part = &p->primary[i];
		uint32_t start, size;
		char tmp[64];

		start = le32_to_cpu(part->start);
		size = le32_to_cpu(part->size);
		if (size == 0)
			break;

		printf("/dev/sda%-2d ", i);
		printf("%s ", part->boot_ind & 0x80 ? "boot" : "    ");
		printf("%8d %8d %8d ", start, start + size - 1, size);

		if (!(fdisk_flags & FDISK_FLAG_x)) {
			string_lba_size(tmp, sizeof(tmp), size);
			printf("%5s ", tmp);
		}

		string_dos_partition_sys_ind(tmp, sizeof(tmp), part->sys_ind);
		printf("%2x %-8s ", part->sys_ind, tmp);

		if (fdisk_flags & FDISK_FLAG_x) {
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
