/*
 * simple dd helper library
 * qianfan Zhao
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "imgeditor.h"

/* @fdsrc: the source file descriptor, negative number means copy from /dev/zero
 * @fddst: the target file descriptor, negative number means write to /dev/null
 */
uint64_t dd64(int fdsrc, int fddst, off64_t offt_src, off64_t offt_dst, uint64_t sz,
	      void (*bufscan)(uint8_t *buf, size_t sz_buster, void *p),
	      void *private_data)
{
	size_t dd_max_bufsz = 1 << 20; /* 1MiB */
	uint64_t n_copied_bytes = 0;
	uint8_t *buffer;

	if (fdsrc < 0 && fddst < 0)
		return n_copied_bytes;

	buffer = malloc(dd_max_bufsz);
	if (!buffer)
		return n_copied_bytes;

	if (!(fdsrc < 0))
		fileseek(fdsrc, offt_src);
	if (!(fddst < 0))
		fileseek(fddst, offt_dst);

	while (sz > 0) {
		size_t sz_buster = dd_max_bufsz;
		int lensrc, lendst;

		if (sz < sz_buster)
			sz_buster = sz;

		if (!(fdsrc < 0)) {
			lensrc = read(fdsrc, buffer, sz_buster);
			if (lensrc <= 0)
				break;
		} else {
			memset(buffer, 0, sz_buster);
			lensrc = sz_buster;
		}

		if (bufscan)
			bufscan(buffer, sz_buster, private_data);

		if (!(fddst < 0)) {
			lendst = write(fddst, buffer, lensrc);
			if (lendst != lensrc)
				break;
		}

		n_copied_bytes += lensrc;
		sz -= lensrc;
	}

	free(buffer);
	return n_copied_bytes;
}

size_t dd(int fd_src, int fd_dst, off_t offt_src, off_t offt_dst, size_t sz,
	  void (*bufscan)(uint8_t *buf, size_t sz_buster, void *p),
	  void *private_data)
{
	return (size_t)dd64(fd_src, fd_dst, offt_src, offt_dst, sz,
			    bufscan, private_data);
}
