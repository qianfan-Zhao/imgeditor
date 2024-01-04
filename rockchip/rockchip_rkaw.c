/*
 * rockchip afptool(RKAW) image editor
 * qianfan Zhao 2022-11-02
 */
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "imgeditor.h"
#include "rockchip.h"

#define RKAW_PAGESIZE		2048

struct rkaw_item {
	char			name[32];
	char			file[60];
	/* the total sector size of the gpt partition */
	uint32_t		part_size;
	/* the image's location in RKAW in bytes, padding to 2K */
	uint32_t		offset;
	/* the offset location sector of the gpt partition */
	uint32_t		flash_offset;
	/* the total aligned page size this image used, pagesize always 2K */
	uint32_t		usespace;
	/* the actually length of this image in bytes */
	uint32_t		size;
};

#define RKAW_MAGIC		0x46414b52 /* RKAW */
#define RKAW_ITEMS		16

struct rkaw_header {
	uint32_t		magic;
	uint32_t		size;
	char			machine_model[34];
	char			machine_id[30];
	char			manufacture[60];
	uint32_t		version;
	uint32_t		n_items;
	struct rkaw_item	items[RKAW_ITEMS];
};

struct rkaw_editor_private_data {
	struct rkaw_header	header;
	int			fd_zero;
	int			fd_null;
};

static int rkaw_init(void *private_data)
{
	struct rkaw_editor_private_data *p = private_data;

	p->fd_zero = fileopen("/dev/zero", O_RDWR, 0);
	if (p->fd_zero < 0)
		return p->fd_zero;

	p->fd_null = fileopen("/dev/null", O_RDWR, 0);
	if (p->fd_null < 0) {
		close(p->fd_zero);
		return p->fd_null;
	}

	return 0;
}

static void rkaw_exit(void *private_data)
{
	struct rkaw_editor_private_data *p = private_data;

	close(p->fd_zero);
	close(p->fd_null);
}

static int is_printable_string(const char *s, size_t sz)
{
	for (size_t i = 0; i < sz && s[i] != '\0'; i++) {
		if (!isprint(s[i]))
			return 0; /* false */
	}

	return 1; /* true */
}

static void rkaw_calc_crc32(uint8_t *buf, size_t sz_buster, void *p)
{
	uint32_t *p_crc = p;

	*p_crc = crc32_rk(*p_crc, buf, sz_buster);
}

static int rkaw_detect(void *private_data, int force_type, int fd)
{
	struct rkaw_editor_private_data *p = private_data;
	struct rkaw_header *header = &p->header;
	long filesize = filelength(fd);
	uint32_t crc1 = 0, crc2 = 0;
	int ret;

	ret = read(fd, header, sizeof(*header));
	if (ret < 0)
		return ret;

	if (header->magic != RKAW_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match(%08x)\n",
				      header->magic);
		return -1;
	}

	if (!is_printable_string(header->machine_model, sizeof(header->machine_model))) {
		fprintf_if_force_type("Error: bad machine_model\n");
		return -1;
	}

	if (!is_printable_string(header->machine_id, sizeof(header->machine_id))) {
		fprintf_if_force_type("Error: bad machine_id\n");
		return -1;
	}

	if (!is_printable_string(header->manufacture, sizeof(header->manufacture))) {
		fprintf_if_force_type("Error: bad manufacture\n");
		return -1;
	}

	if (header->n_items > RKAW_ITEMS) {
		fprintf_if_force_type("Error: bigger item number %d\n", header->n_items);
		return -1;
	}

	for (uint32_t i = 0; i < header->n_items; i++) {
		struct rkaw_item *item = &header->items[i];

		if (!is_printable_string(item->name, sizeof(item->name))) {
			fprintf_if_force_type("Error: bad item name\n");
			return -1;
		}

		if (!is_printable_string(item->file, sizeof(item->file))) {
			fprintf_if_force_type("Error: bad item file\n");
			return -1;
		}

		if (item->offset + item->size > (unsigned)filesize) {
			fprintf_if_force_type("Error: item overflow\n");
			return -1;
		}
	}

	/* header->size + sizeof(crc) should be file length */
	if (header->size + sizeof(crc1) != (unsigned)filesize) {
		fprintf_if_force_type("Error: header->size doesn't match\n");
		return -1;
	}

	lseek(fd, 0, SEEK_SET);
	dd(fd, p->fd_null, 0, 0, header->size, rkaw_calc_crc32, &crc1);
	lseek(fd, header->size, SEEK_SET);
	read(fd, &crc2, sizeof(crc2));
	if (crc1 != crc2) {
		fprintf_if_force_type("Error: CRC doesn't match\n");
		return -1;
	}

	return 0;
}

static int rkaw_list(void *private_data, int fd, int argc, char **argv)
{
	struct rkaw_editor_private_data *p = private_data;
	struct rkaw_header *header = &p->header;

	printf("size:             %d\n", header->size);
	printf("machine_model:    %s\n", header->machine_model);
	printf("machine_id:       %s\n", header->machine_id);
	printf("manufacture:      %s\n", header->manufacture);
	printf("version:          %08x\n", header->version);
	printf("\n");

	for (uint32_t i = 0; i < header->n_items; i++) {
		struct rkaw_item *item = &header->items[i];

		printf("[%02d]: %s (%s)\n", i, item->file, item->name);
		printf("  part_size:      0x%08x\n", item->part_size);
		printf("  offset:         0x%08x\n", item->offset);
		printf("  flash_offset:   0x%08x\n", item->flash_offset);
		printf("  usespace:       %d\n", item->usespace);
		printf("  size:           0x%08x\n", item->size);
	}

	return 0;
}

static int mkparentdir(const char *filepath)
{
	char shellcmd[2048];
	int ret;

	snprintf(shellcmd, sizeof(shellcmd),
		"filepath=%s\n"
		"filedir=$(dirname ${filepath})\n"
		"mkdir -p ${filedir}\n"
		, filepath);
	ret = system(shellcmd);
	if (ret != 0) {
		fprintf(stderr, "Error: create parent dir of %s failed\n",
			filepath);
		return -1;
	}

	return 0;
}

static int rkaw_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	struct rkaw_editor_private_data *p = private_data;
	struct rkaw_header *header = &p->header;
	cJSON *json_items, *json_root = cJSON_CreateObject();
	char tmpbuf[1024];
	int ret;

	if (!json_root)
		return -1;

	jsonputs(json_root, machine_model, header);
	jsonputs(json_root, machine_id, header);
	jsonputs(json_root, manufacture,header);
	jsonputx32(json_root, version, header);

	json_items = cJSON_AddArrayToObject(json_root, "files");
	if (!json_items) {
		cJSON_Delete(json_root);
		return -1;
	}

	for (uint32_t i = 0; i < header->n_items; i++) {
		struct rkaw_item *item = &header->items[i];
		struct cJSON *json;
		size_t n;
		int fd_child;

		json = cJSON_CreateObject();
		if (!json) {
			cJSON_Delete(json_root);
			return -1;
		}
		cJSON_AddItemToArray(json_items, json);

		jsonputs(json, name, item);
		jsonputs(json, file, item);
		jsonputx32(json, part_size, item);
		jsonputx32(json, flash_offset, item);
		jsonputx32(json, usespace, item);

		/* the path of item->file maybe containe directory
		 * such as Image/kernel.img.
		 * let's create it first
		 */
		snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", outdir, item->file);
		ret = mkparentdir(tmpbuf);
		if (ret < 0)
			return ret;

		fd_child = fileopen(tmpbuf, O_WRONLY | O_CREAT, 0664);
		if (fd_child < 0)
			return fd_child;

		ftruncate(fd_child, 0);

		printf("saving %s\n", item->file);
		n = dd(fd, fd_child, item->offset, 0, item->size, NULL, NULL);
		if (n != item->size) {
			fprintf(stderr, "Error: save %s failed\n", item->file);
			return -1;
		}

		close(fd_child);
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "%s/rkaw.json", outdir);
	ret = json_saveto_file(json_root, tmpbuf);
	cJSON_Delete(json_root);

	return ret;
}

static int rkaw_pack(void *private_data, const char *dir, int fd_outimg, int argc, char **argv)
{
	struct rkaw_editor_private_data *p = private_data;
	struct rkaw_header head = { .magic = RKAW_MAGIC };
	cJSON *json_root, *json_items, *json;
	uint32_t offset = aligned_length(sizeof(head), RKAW_PAGESIZE);
	uint32_t crc = 0;
	char tmpfile[1024];
	int ret = -1;

	snprintf(tmpfile, sizeof(tmpfile), "%s/rkaw.json", dir);
	json_root = json_from_file(tmpfile);
	if (!json_root) {
		fprintf(stderr, "Error: load %s failed\n", tmpfile);
		return -1;
	}

	json_items = cJSON_GetObjectItem(json_root, "files");
	if (!json_items) {
		fprintf(stderr, "Error: [files] is not defined in %s\n", tmpfile);
		goto done;
	} else if (!cJSON_IsArray(json_items)) {
		fprintf(stderr, "Error: [files] in %s is not a array\n", tmpfile);
		goto done;
	} else if (cJSON_GetArraySize(json_items) > RKAW_ITEMS) {
		fprintf(stderr, "Error: RKAW only support %d items\n", RKAW_ITEMS);
		goto done;
	}

	jsonloads(json_root, machine_model, &head);
	jsonloads(json_root, machine_id, &head);
	jsonloads(json_root, manufacture, &head);
	jsonloadx32(json_root, version, &head);

	cJSON_ArrayForEach(json, json_items) {
		struct rkaw_item *item = &head.items[head.n_items];
		uint32_t aligned_size, padding;
		int fd_child;
		long size;
		size_t n;

		jsonloads(json, name, item);
		jsonloads(json, file, item);
		jsonloadx32(json, part_size, item);
		jsonloadx32(json, flash_offset, item);

		if (item->name[0] == '\0') {
			fprintf(stderr, "Error: item name is not defined\n");
			goto done;
		} else if (item->file[0] == '\0') {
			fprintf(stderr, "Error: item file is not defined\n");
			goto done;
		}

		snprintf(tmpfile, sizeof(tmpfile), "%s/%s", dir, item->file);
		fd_child = fileopen(tmpfile, O_RDONLY, 0);
		if (fd_child < 0)
			goto done;

		size = filelength(fd_child);
		item->offset = offset;
		item->size = (uint32_t)size;
		aligned_size = aligned_length(item->size, RKAW_PAGESIZE);
		padding = aligned_size - item->size;
		item->usespace = aligned_size / RKAW_PAGESIZE;

		n = dd(fd_child, fd_outimg, 0, offset, item->size, NULL, NULL);
		if (n != item->size) {
			fprintf(stderr, "Error: pack %s failed (write only %zu bytes)\n",
				item->file, n);
			goto done;
		}

		/* padding zero */
		if (padding) {
			n = dd(p->fd_zero, fd_outimg, 0, offset + item->size,
				padding, NULL, NULL);
			if (n != padding) {
				fprintf(stderr, "Error: padding %s failed\n",
					item->file);
				goto done;
			}
		}

		close(fd_child);
		++head.n_items;
		offset += aligned_size;
	}

	head.size = offset;

	/* write the header */
	lseek(fd_outimg, 0, SEEK_SET);
	write(fd_outimg, &head, sizeof(head));
	fsync(fd_outimg);

	/* appending the crc value */
	dd(fd_outimg, p->fd_null, 0, 0, head.size, rkaw_calc_crc32, &crc);
	lseek(fd_outimg, head.size, SEEK_SET);
	write(fd_outimg, &crc, sizeof(crc));
	ret = 0;

done:
	cJSON_Delete(json_root);
	return ret;
}

static struct imgeditor rkaw_editor = {
	.name			= "rkaw",
	.descriptor		= "rockchip afptool(RKAW) image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct rkaw_header),
	.private_data_size	= sizeof(struct rkaw_editor_private_data),
	.init			= rkaw_init,
	.detect			= rkaw_detect,
	.list			= rkaw_list,
	.unpack			= rkaw_unpack,
	.pack			= rkaw_pack,
	.exit			= rkaw_exit,
};

REGISTER_IMGEDITOR(rkaw_editor);
