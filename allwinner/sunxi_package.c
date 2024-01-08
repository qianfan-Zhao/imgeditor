/*
 * sunxi-package.fex(toc1) image editor
 * qianfan Zhao <qianfanguijin@163.com>
 * 2024-01-08
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "imgeditor.h"
#include "structure.h"

#define TOC1_MAGIC			0x89119800
#define TOC1_HEAD_END			0x3b45494d
#define TOC1_ITEM_END			0x3b454949
#define STAMP_VALUE			0x5F0A6C39

struct __attribute__((packed)) toc1_head_info {
	char		name[16];
	__le32		magic;
	__le32		sum;
	__le32		serial_num;
	__le32		status;
	__le32		n_items;
	__le32		valid_len;
	__le32		version_major;
	__le32		version_minor;
	__le32		reversed[3];
	__le32		end;
};

static const struct structure_item structure_toc1_head_info[] = {
	STRUCTURE_ITEM(struct toc1_head_info,	name,		structure_item_print_string),
	STRUCTURE_ITEM(struct toc1_head_info,	serial_num,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct toc1_head_info,	status,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct toc1_head_info,	n_items,	structure_item_print_unsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct toc1_head_info,	valid_len,	structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct toc1_head_info,	version_major,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct toc1_head_info,	version_minor,	structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

struct __attribute__((packed)) toc1_item_info {
	char		name[64];
	__le32		offset;
	__le32		length;
	__le32		encrypt;
	__le32		type;
	__le32		run_addr;
	__le32		index;
	__le32		reversed[69];
	__le32		end;
};

static const struct structure_item structure_toc1_item_info[] = {
	STRUCTURE_ITEM(struct toc1_item_info,	name,		structure_item_print_string),
	STRUCTURE_ITEM(struct toc1_item_info,	offset,		structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct toc1_item_info,	length,		structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct toc1_item_info,	encrypt,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct toc1_item_info,	type,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct toc1_item_info,	run_addr,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct toc1_item_info,	index,		structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

struct sunxi_package_private {
	struct toc1_head_info		head;

	struct toc1_item_info		*items;
	uint32_t			n_items;
};

static void sunxi_package_exit(void *private_data)
{
	struct sunxi_package_private *p = private_data;

	free(p->items);
}

static void sunxi_package_update_checksum(uint8_t *buf, size_t buster_sz,
					  void *arg)
{
	uint32_t *sum = arg, *buf32 = (uint32_t *)buf, remain[4] = { 0 };
	size_t i = 0;

	for (; i < buster_sz / sizeof(*buf32); i++)
		(*sum) += buf32[i];

	if (buster_sz & 3) { /* not aligned */
		memcpy(remain, &buf[i * sizeof(*buf32)], buster_sz & 3);
		(*sum) += *((uint32_t *)remain);
	}
}

static int sunxi_package_detect(void *private_data, int force_type, int fd)
{
	struct sunxi_package_private *p = private_data;
	int64_t total_length = filelength(fd);
	uint32_t sum = STAMP_VALUE;
	int ret;

	ret = read(fd, &p->head, sizeof(p->head));
	if (ret < 0)
		return ret;

	if (le32_to_cpu(p->head.magic) != TOC1_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	if (le32_to_cpu(p->head.end) != TOC1_HEAD_END) {
		fprintf_if_force_type("Error: end magic doesn't match\n");
		return -1;
	}

	dd(fd, -1, 0, -1, le32_to_cpu(p->head.valid_len),
	   sunxi_package_update_checksum, &sum);

	sum -= le32_to_cpu(p->head.sum);
	if (sum != le32_to_cpu(p->head.sum)) {
		fprintf_if_force_type("Error: checksum doesn't match\n");
		return -1;
	}

	/* alloc and loading items */
	p->n_items = le32_to_cpu(p->head.n_items);
	p->items = calloc(p->n_items, sizeof(*p->items));
	if (!p->items) {
		fprintf(stderr, "Error: malloc buffer for %d items failed\n",
			p->n_items);
		return -1;
	}

	lseek(fd, sizeof(struct toc1_head_info), SEEK_SET);
	ret = read(fd, p->items, p->n_items * sizeof(*p->items));
	if (ret != (int)(p->n_items * sizeof(*p->items))) {
		fprintf_if_force_type("Error: read %d items failed\n",
				      p->n_items);
		free(p->items);
		return -1;
	}

	/* check items */
	for (uint32_t i = 0; i < p->n_items; i++) {
		struct toc1_item_info *item = &p->items[i];
		uint32_t offset, length;

		if (le32_to_cpu(item->end) != TOC1_ITEM_END) {
			fprintf_if_force_type("Error: TOC1 item %d's end "
					      "magic doesn't match\n",
					      i);
			free(p->items);
			return -1;
		}

		offset = le32_to_cpu(item->offset);
		length = le32_to_cpu(item->length);
		if (offset + length > total_length) {
			fprintf(stderr, "Error: %s is overflow. offset = %08x "
				"length = %08x\n",
				item->name, offset, length);
			free(p->items);
			return -1;
		}
	}

	return 0;
}

#define PRINT_LEVEL0			"%-20s: "
#define PRINT_LEVEL1			"    %-16s: "

static int sunxi_package_list_verbose(struct sunxi_package_private *p)
{
	structure_print(PRINT_LEVEL0, &p->head, structure_toc1_head_info);

	for (uint32_t i = 0; i < p->n_items; i++) {
		struct toc1_item_info *item = &p->items[i];

		printf("\n");
		structure_print(PRINT_LEVEL1, item, structure_toc1_item_info);
	}

	return 0;
}

static int sunxi_package_list(void *private_data, int fd, int argc, char **argv)
{
	struct sunxi_package_private *p = private_data;

	if (get_verbose_level() > 0)
		return sunxi_package_list_verbose(p);

	printf("name:    %s\n", p->head.name);
	printf("serial:  %08x\n", le32_to_cpu(p->head.serial_num));
	printf("version: %08x%08x\n",
		le32_to_cpu(p->head.version_major),
		le32_to_cpu(p->head.version_minor));

	printf("\n");
	printf("%-10s %-10s %-10s %-10s %-7s %-4s %-5s\n",
		"name", "offset", "length", "run-addr",
		"encrypt", "type", "index");

	for (uint32_t i = 0; i < p->n_items; i++) {
		struct toc1_item_info *item = &p->items[i];

		printf("%-10s 0x%08x 0x%08x 0x%08x     %d    %d    %d\n",
			item->name,
			le32_to_cpu(item->offset),
			le32_to_cpu(item->length),
			le32_to_cpu(item->run_addr),
			le32_to_cpu(item->encrypt),
			le32_to_cpu(item->type),
			le32_to_cpu(item->index));
	}

	return 0;
}

static int sunxi_package_save_json(struct sunxi_package_private *p,
				   const char *filename)
{
	cJSON *items, *root = cJSON_CreateObject();
	int ret = 0;

	if (!root)
		return -1;

	ret = structure_save_json(root, &p->head, structure_toc1_head_info);
	if (ret < 0)
		goto done;

	items = cJSON_AddArrayToObject(root, "items");
	if (!items) {
		ret = -1;
		goto done;
	}

	for (uint32_t i = 0; i < p->n_items; i++) {
		cJSON *json = cJSON_CreateObject();

		if (!json) {
			ret = -1;
			break;
		}

		ret = structure_save_json(json, &p->items[i],
					  structure_toc1_item_info);
		if (ret < 0) {
			cJSON_Delete(json);
			break;
		}

		cJSON_AddItemToArray(items, json);
	}

	if (ret == 0)
		ret = json_saveto_file(root, filename);
done:
	cJSON_Delete(root);
	return ret;
}

static int sunxi_package_load_json(struct sunxi_package_private *p,
				   const char *filename)
{
	cJSON *items, *root = json_from_file(filename);
	int ret = -1;

	if (!root)
		return -1;

	items = cJSON_GetObjectItem(root, "items");
	if (!items) {
		fprintf(stderr, "Error: <items> is not found\n");
		goto done;
	} else if (!cJSON_IsArray(items)) {
		fprintf(stderr, "Error: <items> is not an array\n");
		goto done;
	}

	ret = structure_load_json(root, &p->head, structure_toc1_head_info);
	if (ret < 0)
		goto done;

	p->head.sum = 0;
	p->head.valid_len = 0;
	p->head.magic = cpu_to_le32(TOC1_MAGIC);
	p->head.end = cpu_to_le32(TOC1_HEAD_END);

	p->n_items = cJSON_GetArraySize(items);
	p->head.n_items = cpu_to_le32(p->n_items);

	p->items = calloc(p->n_items, sizeof(*p->items));
	if (!p->items) {
		fprintf(stderr, "Error: alloc %d items failed\n",
			p->n_items);
		ret = -1;
		goto done;
	}

	for (uint32_t i = 0; i < p->n_items; i++) {
		struct toc1_item_info *item = &p->items[i];
		cJSON *json = cJSON_GetArrayItem(items, i);

		ret = structure_load_json(json, item, structure_toc1_item_info);
		if (ret < 0)
			goto done;

		/* discard this value from json configuration if has */
		item->offset = 0;
		item->length = 0;

		if (item->name[0] == '\0') {
			fprintf(stderr, "Error: item %d doesn't has name\n", i);
			ret = -1;
			goto done;
		}

		item->end = cpu_to_le32(TOC1_ITEM_END);
	}

done:
	cJSON_Delete(root);
	return ret;
}

static int sunxi_package_unpack(void *private_data, int fd, const char *outdir,
				int argc, char **argv)
{
	struct sunxi_package_private *p = private_data;
	char filename[256];
	int ret;

	snprintf(filename, sizeof(filename), "%s/sunxi_package.json", outdir);
	ret = sunxi_package_save_json(p, filename);
	if (ret < 0)
		return ret;

	for (uint32_t i = 0; i < p->n_items; i++) {
		struct toc1_item_info *item = &p->items[i];
		uint32_t offset, length;
		int outfd;

		snprintf(filename, sizeof(filename), "%s/%s",
			 outdir, item->name);

		outfd = fileopen(filename, O_RDWR | O_CREAT, 0664);
		if (outfd < 0)
			return outfd;

		offset = le32_to_cpu(item->offset);
		length = le32_to_cpu(item->length);

		dd(fd, outfd, offset, 0, length, NULL, NULL);
		close(outfd);
	}

	return 0;
}

#define SUNXI_PACKAGE_ITEM_ALIGN_LENGTH		1024
#define SUNXI_PACKAGE_ALIGN_LENGTH		16384

static int sunxi_package_pack(void *private_data, const char *dir,
			      int fd_outimg, int argc, char **argv)
{
	uint32_t imgoffset = 0xc00; /* maybe a fixed value */
	struct sunxi_package_private *p = private_data;
	uint32_t sum = STAMP_VALUE;
	uint8_t zero = 0;
	char filename[256];
	int ret;

	snprintf(filename, sizeof(filename), "%s/sunxi_package.json", dir);
	ret = sunxi_package_load_json(p, filename);
	if (ret < 0)
		return ret;

	for (uint32_t i = 0; i < p->n_items; i++) {
		struct toc1_item_info *item = &p->items[i];
		uint32_t length;
		int fd;

		snprintf(filename, sizeof(filename), "%s/%s",
			 dir, item->name);
		fd = fileopen(filename, O_RDONLY, 0);
		if (fd < 0)
			return fd;

		length = (uint32_t)filelength(fd);
		item->offset = cpu_to_le32(imgoffset);
		item->length = cpu_to_le32(length);

		/* write toc1_item_head */
		lseek(fd_outimg, sizeof(p->head) + i * sizeof(*item), SEEK_SET);
		write(fd_outimg, item, sizeof(*item));
		sunxi_package_update_checksum((uint8_t *)item,
					      sizeof(*item), &sum);

		/* write item's binary data */
		dd(fd, fd_outimg, 0, imgoffset, length,
			sunxi_package_update_checksum, &sum);

		imgoffset += aligned_length(length,
				SUNXI_PACKAGE_ITEM_ALIGN_LENGTH);

		/* padding the last item */
		if (i == p->n_items - 1) {
			if (length & (SUNXI_PACKAGE_ITEM_ALIGN_LENGTH)) {
				lseek(fd_outimg, imgoffset - sizeof(zero),
					SEEK_SET);
				write(fd_outimg, &zero, sizeof(zero));
			}
		}
	}

	p->head.valid_len =
		aligned_length(imgoffset, SUNXI_PACKAGE_ALIGN_LENGTH);
	if (imgoffset & (SUNXI_PACKAGE_ALIGN_LENGTH - 1)) {
		lseek(fd_outimg, p->head.valid_len - sizeof(zero), SEEK_SET);
		write(fd_outimg, &zero, sizeof(zero));
	}

	/* fix the endian */
	p->head.valid_len = cpu_to_le32(p->head.valid_len);

	sunxi_package_update_checksum((uint8_t *)&p->head, sizeof(p->head),
				      &sum);
	p->head.sum = cpu_to_le32(sum);

	lseek(fd_outimg, 0, SEEK_SET);
	write(fd_outimg, &p->head, sizeof(p->head));

	return ret;
}

static struct imgeditor sunxi_package_editor = {
	.name			= "sunxi_package",
	.descriptor		= "sunxi_package(toc1) image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct toc1_head_info),
	.private_data_size	= sizeof(struct sunxi_package_private),
	.detect			= sunxi_package_detect,
	.list			= sunxi_package_list,
	.unpack			= sunxi_package_unpack,
	.pack			= sunxi_package_pack,
	.exit			= sunxi_package_exit,
};
REGISTER_IMGEDITOR(sunxi_package_editor);
