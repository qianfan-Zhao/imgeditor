/*
 * rockchip mkkrnlimg(KRNL) and PARAM image editor
 * qianfan Zhao 2022-11-02
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "imgeditor.h"
#include "rockchip.h"

/* partition reserved for ubl is 4M */
#define KRNL_IMAGE_MAXSIZE		(4 << 20)
#define KRNL_IMAGE_MAGIC		0x4C4E524B /* KRNL */
#define RKPARAM_IMAGE_MAGIC		0x4d524150 /* PARM */
#define KRNL_IMAGE_PATCH_INFO_SIZE	12 /* sizeof(magic) + sizeof(size) + sizeof(crc) */

/*
 * layout of krnl image:
 * magic (4B)
 * exe content size (4B)
 * exe contents
 * crc32 (4B)
 */
struct krnl_image {
	uint32_t			magic;
	uint32_t			size;
	uint32_t			crc;
	uint8_t				*exe;
};

struct krnl_editor_private_data {
	struct krnl_image		img;
	uint32_t			magic;
};

static int krnl_image_from_rawfile(struct krnl_image *img, uint32_t magic,
				   const char *filename)
{
	int ret = -1, fd = fileopen(filename, O_RDONLY, 0);
	long filesize;

	if (fd < 0)
		return fd;

	filesize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	if (filesize > KRNL_IMAGE_MAXSIZE - KRNL_IMAGE_PATCH_INFO_SIZE) {
		fprintf(stderr, "Error: partition size limited\n");
		goto done;
	}

	img->exe = malloc(filesize);
	if (!img->exe) {
		fprintf(stderr, "Error: alloc memory failed\n");
		goto done;
	}

	read(fd, img->exe, filesize);
	img->magic = magic;
	img->size = (uint32_t)filesize;
	img->crc = crc32_rk(0, img->exe, img->size);
	ret = 0;

done:
	close(fd);
	return ret;
}

static int krnl_image_generate(struct krnl_image *img, int fdout)
{
	ftruncate(fdout, 0);
	write(fdout, &img->magic, sizeof(img->magic));
	write(fdout, &img->size, sizeof(img->size));
	write(fdout, img->exe, img->size);
	write(fdout, &img->crc, sizeof(img->crc));

	return 0;
}

static int krnl_image_load(int force_type, int fd, struct krnl_image *img, uint32_t magic)
{
	long filesize = filelength(fd);
	uint32_t crc = 0;

	if (filesize <= KRNL_IMAGE_PATCH_INFO_SIZE) {
		fprintf_if_force_type("Error: filesize too short %ld\n", filesize);
		return -1;
	}

	read(fd, &img->magic, sizeof(img->magic));
	if (img->magic != magic) {
		fprintf_if_force_type("Error: bad magic\n");
		return -1;
	}

	read(fd, &img->size, sizeof(img->size));
	if (img->size != (unsigned)filesize - KRNL_IMAGE_PATCH_INFO_SIZE) {
		fprintf_if_force_type("Error: filesize doesn't match\n");
		return -1;
	} else if (img->size > KRNL_IMAGE_MAXSIZE) {
		fprintf_if_force_type("Error: size limited\n");
		return -1;
	}

	img->exe = malloc(img->size);
	if (!img->exe) {
		fprintf(stderr, "Error: alloc memory failed\n");
		return -1;
	}

	read(fd, img->exe, img->size);
	read(fd, &img->crc, sizeof(img->crc));

	crc = crc32_rk(0, img->exe, img->size);
	if (crc != img->crc) {
		fprintf_if_force_type("Error: crc doesn't match %08x != %08x\n",
				      crc, img->crc);
		free(img->exe);
		img->exe = NULL;
		return -1;
	}

	/* loading successful */
	return 0;
}

static int krnl_image_recovery(struct krnl_image *img, const char *out)
{
	int fd = fileopen(out, O_WRONLY | O_CREAT, 0664);

	if (fd < 0)
		return fd;

	ftruncate(fd, 0);
	write(fd, img->exe, img->size);
	close(fd);

	return 0;
}

static int krnl_detect(void *private_data, int force_type, int fd)
{
	struct krnl_editor_private_data *p = private_data;
	struct krnl_image *img = &p->img;

	return krnl_image_load(force_type, fd, img, p->magic);
}

static int krnl_list(void *private_data, int fd, int argc, char **argv)
{
	struct krnl_editor_private_data *p = private_data;

	switch (p->magic) {
	case KRNL_IMAGE_MAGIC:
		printf("rockchip mkkrnlimg image\n");
		break;
	case RKPARAM_IMAGE_MAGIC:
		printf("rockchip parameter image\n");
		break;
	default:
		return -1;
	}

	return 0;
}

static int krnl_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	struct krnl_editor_private_data *p = private_data;
	struct krnl_image *img = &p->img;

	return krnl_image_recovery(img, outdir);
}

static int krnl_pack(void *private_data, const char *dir, int fd_outimg, int argc, char **argv)
{
	struct krnl_editor_private_data *p = private_data;
	struct krnl_image img = { .magic = 0 };
	int ret;

	ret = krnl_image_from_rawfile(&img, p->magic, dir);
	if (ret < 0)
		return ret;

	ret = krnl_image_generate(&img, fd_outimg);
	free(img.exe);
	return ret;
}

static int krnl_init(void *private_data)
{
	struct krnl_editor_private_data *p = private_data;

	p->magic = KRNL_IMAGE_MAGIC;
	return 0;
}

static void krnl_exit(void *private_data)
{
	struct krnl_editor_private_data *p = private_data;

	if (p->img.exe)
		free(p->img.exe);
}

static struct imgeditor krnl_editor = {
	.name			= "krnl",
	.descriptor		= "rockchip mkkrnlimg(KRNL) image editor",
	.flags			= IMGEDITOR_FLAG_SINGLE_BIN,
	.header_size		= KRNL_IMAGE_PATCH_INFO_SIZE,
	.private_data_size	= sizeof(struct krnl_editor_private_data),
	.init			= krnl_init,
	.detect			= krnl_detect,
	.list			= krnl_list,
	.pack			= krnl_pack,
	.unpack			= krnl_unpack,
	.exit			= krnl_exit,
};

REGISTER_IMGEDITOR(krnl_editor);

/* PARAM is most compatible KRNl but has a difference magic */
static int rkparam_init(void *private_data)
{
	struct krnl_editor_private_data *p = private_data;

	p->magic = RKPARAM_IMAGE_MAGIC;
	return 0;
}

static struct imgeditor rkparam_editor = {
	.name			= "rkparam",
	.descriptor		= "rockchip parameter image editor",
	.flags			= IMGEDITOR_FLAG_SINGLE_BIN,
	.header_size		= KRNL_IMAGE_PATCH_INFO_SIZE,
	.private_data_size	= sizeof(struct krnl_editor_private_data),
	.init			= rkparam_init,
	.detect			= krnl_detect,
	.list			= krnl_list,
	.pack			= krnl_pack,
	.unpack			= krnl_unpack,
	.exit			= krnl_exit,
};

REGISTER_IMGEDITOR(rkparam_editor);
