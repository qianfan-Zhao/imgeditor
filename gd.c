/*
 * global data shared with all plugins
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imgeditor.h"
#include "gd_private.h"

static int fd_gd = -1;
static struct global_data *gd = NULL;

struct global_data *imgeditor_get_gd(void)
{
	assert(gd != NULL);

	return gd;
}

#define IMGEDITOR_GD_SIZE			16384

static const char *imgeditor_gd_id(char *s, size_t sz)
{
	snprintf(s, sz, "imgeditor-%d.gd", getpid());
	return s;
}

int imgeditor_core_setup_gd(void)
{
	char gdid[128];
	int fd, ret;

	static_assert(sizeof(struct global_data) <= IMGEDITOR_GD_SIZE,
		      "global_data overrange");

	imgeditor_gd_id(gdid, sizeof(gdid));

	fd = shm_open(gdid, O_RDWR | O_CREAT, 0660);
	if (fd < 0) {
		fprintf(stderr, "Error: create shm(%s) failed(%m)\n",
			gdid);
		return fd;
	}

	ret = ftruncate(fd, IMGEDITOR_GD_SIZE);
	if (ret < 0) {
		fprintf(stderr, "Error: enlarge shm failed\n");
		goto done;
	}

	gd = mmap(NULL, IMGEDITOR_GD_SIZE, PROT_READ | PROT_WRITE,
		  MAP_SHARED, fd, 0);
	if (gd == MAP_FAILED) {
		fprintf(stderr, "Error: mmap gd failed\n");
		ret = -1;
		goto done;
	}

	fd_gd = fd;

done:
	if (ret < 0) {
		close(fd);
		shm_unlink(gdid);
	}

	return ret;
}

int imgeditor_plugin_setup_gd(void)
{
	char gdid[128];
	int fd;

	imgeditor_gd_id(gdid, sizeof(gdid));

	fd = shm_open(gdid, O_RDWR, 0660);
	if (fd < 0) {
		fprintf(stderr, "Error: plugin open shm %s failed(%m)\n", gdid);
		return fd;
	}

	gd = mmap(NULL, IMGEDITOR_GD_SIZE, PROT_READ | PROT_WRITE,
		  MAP_SHARED, fd, 0);
	if (gd == MAP_FAILED) {
		fprintf(stderr, "Error: plugin mmap gd failed\n");
		close(fd);
		return -1;
	}

	fd_gd = fd;
	return 0;
}

void imgeditor_free_gd(void)
{
	char gdid[128];

	if (gd) {
		munmap(gd, IMGEDITOR_GD_SIZE);
		close(fd_gd);
		shm_unlink(imgeditor_gd_id(gdid, sizeof(gdid)));

		gd = NULL;
		fd_gd = -1;
	}
}

int get_verbose_level(void)
{
	return imgeditor_get_gd()->verbose_level;
}
