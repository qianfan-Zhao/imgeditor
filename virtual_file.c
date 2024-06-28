/*
 * create a new virtual file from part of a file
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <string.h>
#include "imgeditor.h"
#include "gd_private.h"

static struct virtual_file *virtual_file_get(int fd)
{
	struct global_data *gd = imgeditor_get_gd();
	struct virtual_file *vfps = gd->vfps;

	for (int i = 0; i < MAX_VIRTUAL_FILE; i++) {
		struct virtual_file *vfp = &vfps[i];

		if (vfp->used && vfp->fd == fd)
			return vfp;
	}

	return NULL;
}

int fileopen(const char *file, int flags, mode_t mode)
{
	int fd = open(file, flags, mode);

	if (fd < 0)
		fprintf(stderr, "Error: open %s failed(%m)\n", file);

	return fd;
}

int64_t filelength(int fd)
{
	struct virtual_file *vf = virtual_file_get(fd);

	if (vf)
		return vf->total_length;

	/* lseek can't work on 32bit ARM platform if the file is larger than
	 * 2GB, it will report EOVERFLOW.
	 * let's use lseek64 instead of lseek.
	 */
	int64_t sz = lseek64(fd, 0, SEEK_END);

	lseek64(fd, 0, SEEK_SET);
	return sz;
}

off64_t filestart(int fd)
{
	struct virtual_file *vf = virtual_file_get(fd);

	if (!vf)
		return 0;

	return vf->start_offset;
}

off64_t fileseek(int fd, off64_t offset)
{
	return lseek64(fd, filestart(fd) + offset, SEEK_SET);
}

/* Return zero on successful */
int fileread(int fd, void *buf, size_t sz)
{
	size_t n = 0;

	while (n < sz) {
		int ret = read(fd, buf + n, sz - n);

		if (ret < 0)
			return ret;

		if (ret == 0) /* end of file */
			break;

		n += ret;
	}

	return n == sz ? 0 : -1;
}

static struct virtual_file *virtual_file_get_unused()
{
	struct global_data *gd = imgeditor_get_gd();
	struct virtual_file *vfps = gd->vfps;

	for (int i = 0; i < MAX_VIRTUAL_FILE; i++) {
		struct virtual_file *vfp = &vfps[i];

		if (!vfp->used) {
			vfp->used = 1;
			return vfp;
		}
	}

	return NULL;
}

static int64_t filelength_no_cache(int fd)
{
	/* lseek can't work on 32bit ARM platform if the file is larger than
	 * 2GB, it will report EOVERFLOW.
	 * let's use lseek64 instead of lseek.
	 */
	return lseek64(fd, 0, SEEK_END);
}

int virtual_file_dup(int ref_fd, off64_t offset)
{
	struct virtual_file *vfp = virtual_file_get_unused();

	vfp->fd = dup(ref_fd);
	if (vfp->fd < 0) {
		fprintf(stderr, "Error: dup %d failed\n", ref_fd);
		vfp->used = 0;
		return vfp->fd;
	}

	vfp->start_offset = offset + filestart(ref_fd);
	vfp->total_length =
		filelength_no_cache(ref_fd) - vfp->start_offset;

	lseek64(vfp->fd, vfp->start_offset, SEEK_SET);
	return vfp->fd;
}

int virtual_file_open(const char *filename, int flags, mode_t t, off64_t offset)
{
	int fd = fileopen(filename, flags, t);
	struct virtual_file *vfp;

	if (fd < 0)
		return fd;

	vfp = virtual_file_get_unused();
	if (!vfp) {
		close(fd);
		return -1;
	}

	vfp->fd = fd;
	vfp->start_offset = offset;
	vfp->total_length =
		filelength_no_cache(fd) - vfp->start_offset;

	lseek64(vfp->fd, vfp->start_offset, SEEK_SET);
	return vfp->fd;
}

int virtual_file_close(int fd)
{
	struct virtual_file *vfp = virtual_file_get(fd);

	if (!vfp)
		return -1;

	close(vfp->fd);
	memset(vfp, 0, sizeof(*vfp));
	vfp->fd = -1;
	return 0;
}
