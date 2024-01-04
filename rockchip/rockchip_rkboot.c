/*
 * rockchip boot_merge image editor tools
 * Reference: rk-uboot:tools/rockchip/boot_merger.c
 * qianfan Zhao
 */
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "imgeditor.h"
#include "rockchip.h"
#include "structure.h"
#include "rc4.h"
#include "xstring.h"

#define RKBOOT_RC4_CHUNK_SIZE		512

static unsigned char rkboot_rc4_key[16] = {
	124,	78,	3,	4,	85,	5,	9,	7,
	45,	44,	123,	56,	23,	13,	23,	17
};

#define RKBOOT_HEAD_MAGIC	0x2052444c /* 'LDR ' */

struct __attribute__((packed)) rkboot_head {
	uint32_t		magic;
	uint16_t		this_struct_size;
	uint32_t		version;
	uint32_t		this_tool_version;
	struct rktime		release_time;
	uint32_t		device;
	uint8_t			entry471_count;
	uint32_t		entry471_offset;
	uint8_t			entry471_size;
	uint8_t			entry472_count;
	uint32_t		entry472_offset;
	uint8_t			entry472_size;
	uint8_t			entry_loader_count;
	uint32_t		entry_loader_offset;
	uint8_t			entry_loader_size;
	uint8_t			sign_flag;
	uint8_t			rc4_flag;
	uint8_t			unused[57];
};

static struct structure_item structure_rkboot_head[] = {
	STRUCTURE_ITEM(struct rkboot_head, magic,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, version,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, this_tool_version,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, release_time,	structure_item_print_rktime),
	STRUCTURE_ITEM(struct rkboot_head, device,		structure_item_print_rkdevice_code),
	STRUCTURE_ITEM(struct rkboot_head, entry471_count,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry471_offset,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry471_size,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry472_count,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry472_offset,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry472_size,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry_loader_count,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry_loader_offset,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, entry_loader_size,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_head, sign_flag,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct rkboot_head, rc4_flag,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

#define RKBOOT_ENTRY471		1
#define RKBOOT_ENTRY472		2
#define RKBOOT_ENTRY_LOADER	4

struct __attribute__((packed)) rkboot_entry {
	uint8_t			this_struct_size;
	uint32_t		entry_type;
	uint16_t		wname[20]; /* WCHAR name */
	uint32_t		data_offset;
	uint32_t		data_size;
	uint32_t		data_delay; /* second */
};

static void structure_item_print_rkboot_entry(const char *print_name_fmt,
					      const char *name,
					      const void *addr, size_t sz)
{
	uint32_t entry_type = *((uint32_t *)addr);

	structure_print_name(print_name_fmt, name);

	switch (entry_type) {
	case RKBOOT_ENTRY471:
		printf("Entry 471\n");
		break;
	case RKBOOT_ENTRY472:
		printf("Entry 472\n");
		break;
	case RKBOOT_ENTRY_LOADER:
		printf("Entry loader\n");
		break;
	default:
		printf("Entry 0x%08x\n", entry_type);
		break;
	}
}

static void structure_item_print_rkboot_wname(const char *print_name_fmt,
					      const char *name,
					      const void *addr, size_t sz)
{
	const char *wname = addr;

	structure_print_name(print_name_fmt, name);

	/* wname doesn't include any Chinese code */
	for (size_t i = 0; i < sz && wname[i] != '\0'; i += 2)
		printf("%c", wname[i]);
	printf("\n");
}

static struct structure_item structure_rkboot_entry[] = {
	STRUCTURE_ITEM(struct rkboot_entry, entry_type,		structure_item_print_rkboot_entry),
	STRUCTURE_ITEM(struct rkboot_entry, wname,		structure_item_print_rkboot_wname),
	STRUCTURE_ITEM(struct rkboot_entry, data_offset,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkboot_entry, data_size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct rkboot_entry, data_delay,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

struct rkboot_editor_private_data {
	struct rkboot_head	head;

	struct rkboot_entry	*entrys;
	uint32_t		total_entrys;
};

static void rkboot_exit(void *private_data)
{
	struct rkboot_editor_private_data *p = private_data;

	free(p->entrys);
}

static int rkboot_detect(void *private_data, int force_type, int fd)
{
	struct rkboot_editor_private_data *p = private_data;
	struct rkboot_head *head = &p->head;
	int ret;

	static_assert(sizeof(struct rktime) == 7, "struct rktime");
	static_assert(sizeof(struct rkboot_head) == 0x66, "struct rkboot_head");
	static_assert(sizeof(struct rkboot_entry) == 0x39, "struct rkboot_entry");

	ret = read(fd, head, sizeof(*head));
	if (ret < 0) {
		fprintf_if_force_type("Error: read head failed\n");
		return ret;
	}

	if (head->magic != RKBOOT_HEAD_MAGIC) {
		fprintf_if_force_type("Error: bad magic %08x\n", head->magic);
		return -1;
	}

	if (head->this_struct_size != sizeof(*head)) {
		fprintf_if_force_type("Error: structure size doesn't match: %d\n",
				       head->this_struct_size);
		return -1;
	}

	p->total_entrys = head->entry471_count
			+ head->entry472_count
			+ head->entry_loader_count;
	if (p->total_entrys > 10) {
		fprintf_if_force_type("Error: too much boot entrys\n");
		return -1;
	}

	p->entrys = calloc(p->total_entrys, sizeof(struct rkboot_entry));
	if (!p->entrys) {
		fprintf(stderr, "Error: alloc %d rkboot entrys failed\n",
			p->total_entrys);
		return -1;
	}

	lseek(fd, head->entry471_offset, SEEK_SET);
	ret = read(fd, p->entrys, p->total_entrys * sizeof(struct rkboot_entry));
	if (ret < 0) {
		free(p->entrys);
		return ret;
	}

	return 0;
}

static int rkboot_list(void *private_data, int fd, int argc, char **argv)
{
	struct rkboot_editor_private_data *p = private_data;

	structure_print("%-20s: ", &p->head, structure_rkboot_head);

	for (uint32_t i = 0; i < p->total_entrys; i++) {
		printf("\n");
		structure_print("%-20s: ", &p->entrys[i], structure_rkboot_entry);
	}

	return 0;
}

static void rkboot_file_rc4_decode(uint8_t *buf, size_t sz_buster, void *p)
{
	int rc4_flag = *((int *)p);

	if (rc4_flag) {
		while (sz_buster > 0) {
			size_t rc4_chunk_size = RKBOOT_RC4_CHUNK_SIZE;

			if (sz_buster < rc4_chunk_size)
				rc4_chunk_size = sz_buster;

			rc4_encode(buf, rc4_chunk_size, rkboot_rc4_key);
			sz_buster -= rc4_chunk_size;
			buf += rc4_chunk_size;
		}
	}
}

static int rkboot_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	struct rkboot_editor_private_data *p = private_data;

	for (uint32_t i = 0; i < p->total_entrys; i++) {
		struct rkboot_entry *entry = &p->entrys[i];
		char name[sizeof(entry->wname)] = { 0 };
		char outname[1024] = { 0 };
		int rc4_flag;
		size_t n;
		int fd_child;

		rc4_flag = p->head.rc4_flag &&
			entry->entry_type == RKBOOT_ENTRY_LOADER;

		/* convert wname to name */
		for (size_t j = 0; j < sizeof(entry->wname) / sizeof(entry->wname[0]); j++) {
			if (entry->wname[j] == '\0')
				break;
			name[j] = (char)entry->wname[j];
		}

		snprintf(outname, sizeof(outname), "%s/%s", outdir, name);
		fd_child = fileopen(outname, O_RDWR | O_CREAT, 0664);
		if (fd_child < 0)
			return fd_child;

		printf("save %s\n", name);
		n = dd(fd, fd_child, entry->data_offset, 0, entry->data_size,
		       rkboot_file_rc4_decode, &rc4_flag);
		close(fd_child);

		if (n != entry->data_size) {
			fprintf(stderr, "Error: save %s failed\n", outname);
			return -1;
		}
	}

	return 0;
}

static struct imgeditor rockchip_rkboot = {
	.name			= "rkboot",
	.descriptor		= "rockchip boot_merge image(LDR)",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct rkboot_head),
	.private_data_size	= sizeof(struct rkboot_editor_private_data),
	.detect			= rkboot_detect,
	.list			= rkboot_list,
	.unpack			= rkboot_unpack,
	.exit			= rkboot_exit,
};

REGISTER_IMGEDITOR(rockchip_rkboot);
