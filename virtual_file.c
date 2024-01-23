/*
 * create a new virtual file from part of a file
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include "imgeditor.h"
#include <string.h>

#define MAX_VIRTUAL_FILE	32

struct virtual_file_private {
	struct virtual_file	vfile;
	int			used;
};

static struct virtual_file_private vfps[MAX_VIRTUAL_FILE];

static struct virtual_file_private *virtual_file_private_get(int fd)
{
	for (int i = 0; i < MAX_VIRTUAL_FILE; i++) {
		struct virtual_file_private *vfp = &vfps[i];

		if (vfp->used && vfp->vfile.fd == fd)
			return vfp;
	}

	return NULL;
}

static struct virtual_file_private *virtual_file_private_get_unused()
{
	for (int i = 0; i < MAX_VIRTUAL_FILE; i++) {
		struct virtual_file_private *vfp = &vfps[i];

		if (!vfp->used) {
			vfp->used = 1;
			return vfp;
		}
	}

	return NULL;
}

struct virtual_file *virtual_file_get(int fd)
{
	struct virtual_file_private *vfp = virtual_file_private_get(fd);

	if (!vfp)
		return NULL;

	return &vfp->vfile;
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
	struct virtual_file_private *vfp = virtual_file_private_get_unused();

	vfp->vfile.fd = dup(ref_fd);
	if (vfp->vfile.fd < 0) {
		fprintf(stderr, "Error: dup %d failed\n", ref_fd);
		vfp->used = 0;
		return vfp->vfile.fd;
	}

	vfp->vfile.start_offset = offset + filestart(ref_fd);
	vfp->vfile.total_length =
		filelength_no_cache(ref_fd) - vfp->vfile.start_offset;

	lseek64(vfp->vfile.fd, vfp->vfile.start_offset, SEEK_SET);
	return vfp->vfile.fd;
}

int virtual_file_open(const char *filename, int flags, mode_t t, off64_t offset)
{
	int fd = fileopen(filename, flags, t);
	struct virtual_file_private *vfp;

	if (fd < 0)
		return fd;

	vfp = virtual_file_private_get_unused();
	if (!vfp) {
		close(fd);
		return -1;
	}

	vfp->vfile.fd = fd;
	vfp->vfile.start_offset = offset;
	vfp->vfile.total_length =
		filelength_no_cache(fd) - vfp->vfile.start_offset;

	lseek64(vfp->vfile.fd, vfp->vfile.start_offset, SEEK_SET);
	return vfp->vfile.fd;
}

int virtual_file_close(int fd)
{
	struct virtual_file_private *vfp = virtual_file_private_get(fd);

	if (!vfp)
		return -1;

	close(vfp->vfile.fd);
	memset(vfp, 0, sizeof(*vfp));
	vfp->vfile.fd = -1;
	return 0;
}
