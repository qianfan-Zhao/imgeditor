/*
 * rockchip rkImageMarker image editor
 *
 * qianfan Zhao 2022-11-03
 */
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "imgeditor.h"
#include "rockchip.h"
#include "gmssl/md5.h"

#define RKIMG_HEAD_MAGIC	0x57464b52 /* RKFW */
#define RKIMG_OSTYPE_ANDROID	1

struct __attribute__((packed)) __attribute__((aligned(2))) rkimage_head {
	uint32_t		magic;
	uint16_t		this_struct_size;
	uint32_t		version;
	uint32_t		this_tool_version;
	struct rktime		release_time;
	uint32_t		device;
	uint32_t		boot_offset;
	uint32_t		boot_size;
	uint32_t		fw_offset;
	uint32_t		fw_size;
	uint32_t		unknown1;
	uint32_t		ostype;
	uint8_t			unused2[49];
	uint32_t		boot_storage;
};

struct rkimage_editor_private_data {
	struct rkimage_head	head;
};

#define RKIMG_JSON		"rkimg.json"
#define RKIMG_BOOTNAME		"boot.bin"
#define RKIMG_FWNAME		"fw.bin"

static void rkimg_update_md5(uint8_t *buf, size_t sz_buster, void *p)
{
	MD5_CTX *ctx = p;

	md5_update(ctx, buf, sz_buster);
}

static int rkimg_detect(void *private_data, int force_type, int fd)
{
	struct rkimage_editor_private_data *p = private_data;
	struct rkimage_head *head = &p->head;
	uint8_t md5sum[MD5_DIGEST_SIZE], md5sum_infile[MD5_DIGEST_SIZE];
	char buf[256] = { 0 };
	long filesize;
	MD5_CTX ctx;
	int ret;

	static_assert(sizeof(struct rktime) == 7, "struct rktime");
	static_assert(sizeof(struct rkimage_head) == 0x66, "struct rkimage_head");

	/* file layout:
	 *
	 * rkimg_head + boot + fw + 32 bytes md5 checksum string.
	 */
	filesize = filelength(fd);

	ret = read(fd, head, sizeof(*head));
	if (ret < 0) {
		fprintf_if_force_type("Error: read head failed\n");
		return ret;
	}

	if (head->magic != RKIMG_HEAD_MAGIC) {
		fprintf_if_force_type("Error: bad magic %08x\n", head->magic);
		return -1;
	}

	if (head->this_struct_size != sizeof(*head)) {
		fprintf_if_force_type("Error: structure size doesn't match: %d\n",
				       head->this_struct_size);
		return -1;
	}

	if (head->this_struct_size + head->boot_size + head->fw_size +
		MD5_DIGEST_SIZE * 2 != (unsigned)filesize) {
		fprintf_if_force_type("Error: file length doesn't match\n");
		return -1;
	}

	md5_init(&ctx);
	dd(fd, -1, 0, 0, filesize - MD5_DIGEST_SIZE * 2, rkimg_update_md5, &ctx);
	read(fd, buf, MD5_DIGEST_SIZE * 2); /* string format */
	md5_finish(&ctx, md5sum);

	xstring(buf, NULL, md5sum_infile, sizeof(md5sum_infile));
	if (memcmp(md5sum, md5sum_infile, sizeof(md5sum))) {
		fprintf_if_force_type("Error: MD5 checksum doesn't match\n");
		return -1;
	}

	return 0;
}

static int rkimg_list(void *private_data, int fd, int argc, char **argv)
{
	struct rkimage_editor_private_data *p = private_data;
	struct rkimage_head *head = &p->head;
	const struct rkdevice_info *info;

	printf("version:   %08x\n", head->version);
	printf("version2:  %08x\n", head->this_tool_version);
	printf("release:   %04d-%02d-%02d %02d:%02d:%02d\n",
		head->release_time.year, head->release_time.mon,
		head->release_time.day, head->release_time.hour,
		head->release_time.min, head->release_time.sec);

	info = get_rkdevice_info(NULL, head->device);
	if (info)
		printf("device:    %s\n", info->name);
	else
		printf("device:    0x%08x\n", head->device);

	printf("boot:      [%08x, %08x]\n", head->boot_offset, head->boot_size);
	printf("fw:        [%08x, %08x]\n", head->fw_offset, head->fw_size);
	printf("storage:   %08x\n", head->boot_storage);

	if (head->ostype == RKIMG_OSTYPE_ANDROID)
		printf("os:        androidos\n");

	return 0;
}

static char hexdumpc(int c, int upper)
{
	c = c & 0x0f;

	if (c <= 9)
		return c + '0';

	return c - 10 + "aA"[!!upper];
}

static size_t hexdump_to_buf(const uint8_t *p, size_t sz, char *buf, size_t bufsz,
			     int upper)
{
	size_t n = 0;

	while (sz-- > 0 && bufsz > 3 /* keep space for null */) {
		uint8_t b = *p++;

		buf[0] = hexdumpc((b >> 4) & 0xf, upper);
		buf[1] = hexdumpc((b >> 0) & 0xf, upper);
		bufsz -= 2;
		buf += 2;
		n += 2;
	}

	*buf = '\0';
	return n;
}

static int rkimg_pack(void *private_data, const char *dir, int fd_outimg, int argc, char **argv)
{
	struct rkimage_head head = { .magic = RKIMG_HEAD_MAGIC };
	uint8_t md5sum[MD5_DIGEST_SIZE];
	MD5_CTX ctx;
	cJSON *json;
	char tmpbuf[1024];
	int fd_boot = -1, fd_fw = -1;
	long sz_boot = 0, sz_fw = 0;
	int ret = -1;

	snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", dir, RKIMG_JSON);
	json = json_from_file(tmpbuf);
	if (!json) {
		fprintf(stderr, "Error: loading %s failed\n", tmpbuf);
		return -1;
	}

	head.this_struct_size = sizeof(head);
	rktime_init(&head.release_time, time(NULL));

	jsonloadx32(json, version, &head);
	jsonloadx32(json, this_tool_version, &head);
	jsonloadx32(json, device, &head);
	jsonloadx32(json, boot_storage, &head);
	jsonloadx32(json, ostype, &head);
	cJSON_Delete(json);

	snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", dir, RKIMG_BOOTNAME);
	fd_boot = fileopen(tmpbuf, O_RDONLY, 0);
	if (fd_boot < 0)
		return fd_boot;

	snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", dir, RKIMG_FWNAME);
	fd_fw = fileopen(tmpbuf, O_RDONLY, 0);
	if (fd_fw < 0) {
		close(fd_boot);
		return fd_fw;
	}

	sz_boot = filelength(fd_boot);
	if (sz_boot < 0) {
		fprintf(stderr, "Error: get length of %s failed\n",
			RKIMG_BOOTNAME);
		goto done;
	}

	sz_fw = filelength(fd_fw);
	if (sz_fw < 0) {
		fprintf(stderr, "Error: get length of %s failed\n",
			RKIMG_FWNAME);
		goto done;
	}

	head.boot_offset = sizeof(head);
	head.boot_size = (uint32_t)sz_boot;
	head.fw_offset = head.boot_offset + head.boot_size;
	head.fw_size = (uint32_t)sz_fw;

	write(fd_outimg, &head, sizeof(head));
	dd(fd_boot, fd_outimg, 0, head.boot_offset, head.boot_size, NULL, NULL);
	dd(fd_fw, fd_outimg, 0, head.fw_offset, head.fw_size, NULL, NULL);
	ret = 0;

	/* appending the md5 string */
	md5_init(&ctx);
	dd(fd_outimg, -1, 0, 0,
	   head.this_struct_size + head.boot_size + head.fw_size,
	   rkimg_update_md5, &ctx);
	md5_finish(&ctx, md5sum);

	hexdump_to_buf(md5sum, sizeof(md5sum), tmpbuf, sizeof(tmpbuf), 0);
	lseek(fd_outimg, head.this_struct_size + head.boot_size + head.fw_size, SEEK_SET);
	write(fd_outimg, tmpbuf, strlen(tmpbuf));

done:
	close(fd_boot);
	close(fd_fw);
	return ret;
}

static int rkimg_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	struct rkimage_editor_private_data *p = private_data;
	struct rkimage_head *head = &p->head;
	cJSON *json = cJSON_CreateObject();
	char filename[1024];
	int ret;

	if (!json)  {
		fprintf(stderr, "Error: creat json object failed\n");
		return -1;
	}

	jsonputx32(json, version, head);
	jsonputx32(json, this_tool_version, head);
	jsonputx32(json, device, head);
	jsonputx32(json, boot_storage, head);
	jsonputx32(json, ostype, head);
	snprintf(filename, sizeof(filename), "%s/%s", outdir, RKIMG_JSON);
	ret = json_saveto_file(json, filename);
	cJSON_Delete(json);
	if (ret < 0)
		return 0;

	for (int i = 0; i < 2; i++) { /* save RKIMG_BOOTNAME and RKIMG_FWNAME */
		uint32_t offset;
		uint32_t size;
		int fd_child;
		size_t n;

		switch (i) {
		case 0:
			snprintf(filename, sizeof(filename), "%s/%s",
				 outdir, RKIMG_BOOTNAME);
			offset = head->boot_offset;
			size = head->boot_size;
			break;
		default:
			snprintf(filename, sizeof(filename), "%s/%s",
				 outdir, RKIMG_FWNAME);
			offset = head->fw_offset;
			size = head->fw_size;
			break;
		}

		fd_child = fileopen(filename, O_WRONLY | O_CREAT, 0664);
		if (fd_child < 0)
			return fd_child;

		ftruncate(fd_child, 0);

		n = dd(fd, fd_child, offset, 0, size, NULL, NULL);
		if (n != size) {
			fprintf(stderr, "Error: save %s failed\n", filename);
			close(fd_child);
			return -1;
		}

		close(fd_child);
	}

	return 0;
}

static struct imgeditor rockchip_rkimg = {
	.name			= "rkfw",
	.descriptor		= "rockchip rkImageMaker image(RKFW)",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct rkimage_head),
	.private_data_size	= sizeof(struct rkimage_editor_private_data),
	.detect			= rkimg_detect,
	.list			= rkimg_list,
	.pack			= rkimg_pack,
	.unpack			= rkimg_unpack,
};

REGISTER_IMGEDITOR(rockchip_rkimg);
