/*
 * bitmask helper interface
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef IMGEDITOR_BITMASK_H
#define IMGEDITOR_BITMASK_H

#include <stdint.h>
#include <stdio.h>

struct bitmask {
	size_t			total_bits;
	size_t			bufsize;
	uint8_t			buffer[0];
};

struct bitmask *alloc_bitmask(size_t total_bits);
void bitmask_free(struct bitmask *b);

int bitmask_write(struct bitmask *b, size_t bit_idx, int v);
int bitmask_set(struct bitmask *b, size_t bit_idx);
int bitmask_set_bits(struct bitmask *b, size_t from_bit, size_t bits);
int bitmask_unset(struct bitmask *b, size_t bit_idx);
int bitmask_not(struct bitmask *b);
int bitmask_memset(struct bitmask *b, uint8_t val);
int bitmask_memcpy(struct bitmask *b, const void *src, size_t src_bytes);
int bitmask_memcpy_lsbfirst(struct bitmask *b, const void *src, size_t src_bytes);
int bitmask_get(struct bitmask *b, size_t bit_idx);
int bitmask_next_zero(struct bitmask *b, size_t from_bit);
int bitmask_next_one(struct bitmask *b, size_t from_bit);

struct bitmask *bitmask_xor(const struct bitmask *a, const struct bitmask *b);

#define bitmask_foreach(i, b) \
	for (int i = bitmask_next_one(b, 0); i >= 0; i = bitmask_next_one(b, i + 1))

struct bitmask_continue_iterator {
	int			start;
	int			bits;
	struct bitmask		*b;

	/* private data, do not touch */
	int			__end;
};

struct bitmask_continue_iterator bitmask_continue_iterator_init(struct bitmask *b);
void bitmask_continue_iterator_next(struct bitmask_continue_iterator *it);

#define bitmask_foreach_continue(it, b) \
for (struct bitmask_continue_iterator it = bitmask_continue_iterator_init(b); \
	!(it.start < 0); \
	bitmask_continue_iterator_next(&it))

#endif
