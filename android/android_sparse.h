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

struct android_sparse_input {
	struct android_sparse_chunk	*chunks;
	size_t				active_count;
	size_t				total_chunk_size;

	size_t				error;
};

int android_sparse_init(struct android_sparse_input *input);

int android_sparse_add_chunk(struct android_sparse_input *input,
			     uint32_t fs_blocknr, uint32_t count);

typedef int (*android_sparse_write_t)(int fd_target,
				      struct android_sparse_chunk *chunk,
				      void *private_data);

/* @blksz: the raw filesystem's block size in bytes
 * @total_blks: how much blocks the raw filesystem has.
 */
int android_sparse_finish(struct android_sparse_input *input,
			  int fd_target,
			  size_t blksz, size_t total_blks,
			  android_sparse_write_t write_cb,
			  void *private_data);

#endif
