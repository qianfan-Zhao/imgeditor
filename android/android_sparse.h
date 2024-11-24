/*
 * android sparse common functions
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef ANDROID_SPARSE_H
#define ANDROID_SPARSE_H

#include <stdio.h>
#include <stdint.h>

struct android_sparse_chunk {
	/* the block number in raw filesystem */
	uint32_t			fs_blocknr;

	/* total block number in this chunk */
	uint32_t			count;
};

/* @blksz: the raw filesystem's block size in bytes
 * @total_blks: how much blocks the raw filesystem has.
 */
struct android_sparse_input {
	size_t				blksz;
	size_t				total_blks;
	size_t				error;
	struct bitmask			*chunk_bitmasks;
};

int android_sparse_init(struct android_sparse_input *input,
			size_t blksz, size_t total_blks);

int android_sparse_add_chunk(struct android_sparse_input *input,
			     uint32_t fs_blocknr, uint32_t count);

typedef int (*android_sparse_write_t)(int fd_target,
				      struct android_sparse_chunk *chunk,
				      void *private_data);

int android_sparse_finish(struct android_sparse_input *input,
			  int fd_target,
			  android_sparse_write_t write_cb,
			  void *private_data);

#endif
