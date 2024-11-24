/*
 * bitmask helper interface
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdlib.h>
#include <string.h>
#include "bitmask.h"

#define aligned_bytes(bits)	(((bits) + 7) / 8)

struct bitmask *alloc_bitmask(size_t total_bits)
{
	size_t bufsize = aligned_bytes(total_bits);
	struct bitmask *b = calloc(1, sizeof(struct bitmask) + bufsize);

	if (b) {
		b->bufsize = bufsize;
		b->total_bits = total_bits;
	}

	return b;
}

void bitmask_free(struct bitmask *b)
{
	free(b);
}

int bitmask_memset(struct bitmask *b, uint8_t c)
{
	if (!b)
		return -1;

	memset(b->buffer, c, b->bufsize);
	return 0;
}

static inline size_t get_bit_index_msbfirst(size_t bit_idx, size_t *byte_idx)
{
	*byte_idx = bit_idx / 8;

	return 7 - bit_idx % 8;
}

int bitmask_write(struct bitmask *b, size_t bit_idx, int v)
{
	uint8_t value = 1;
	size_t byte, bit;

	if (bit_idx >= b->total_bits)
		return -1;

	bit = get_bit_index_msbfirst(bit_idx, &byte);
	value <<= bit;

	if (v)
		b->buffer[byte] |= value;
	else
		b->buffer[byte] &= ~value;

	return 0;
}

static int _bitmask_write_bits(struct bitmask *b, size_t from_bit, size_t bits, int v)
{
	int ret = 0;

	for (size_t i = 0; i < bits; i++)
		ret += bitmask_write(b, from_bit + i, v);

	return ret;
}

int bitmask_set(struct bitmask *b, size_t bit_idx)
{
	return bitmask_write(b, bit_idx, 1);
}

int bitmask_set_bits(struct bitmask *b, size_t from_bit, size_t bits)
{
	return _bitmask_write_bits(b, from_bit, bits, 1);
}

int bitmask_unset(struct bitmask *b, size_t bit_idx)
{
	return bitmask_write(b, bit_idx, 0);
}

int bitmask_unset_bits(struct bitmask *b, size_t from_bit, size_t bits)
{
	return _bitmask_write_bits(b, from_bit, bits, 1);
}

int bitmask_get(struct bitmask *b, size_t bit_idx)
{
	size_t byte, bit;

	if (bit_idx >= b->total_bits)
		return -1;

	bit = get_bit_index_msbfirst(bit_idx, &byte);
	return !!(b->buffer[byte] & (1 << bit));
}

static int _bitmask_first_one(struct bitmask *b, size_t from_bit, uint8_t xor)
{
	for (size_t i = from_bit / 8; i < b->bufsize; i++) {
		uint8_t mask = b->buffer[i] ^ xor;
		size_t bit = i * 8;
		int found = 0;

		if (mask == 0)
			continue;

		for (int j = 7; j >= 0; j--) {
			found = bit >= from_bit && (mask & (1 << j));
			if (found)
				break;
			bit++;
		}

		if (!found) /* continue search the next word */
			continue;

		if (bit >= b->total_bits)
			return -1;

		return bit;
	}

	return -1;
}

int bitmask_next_zero(struct bitmask *b, size_t from_bit)
{
	return _bitmask_first_one(b, from_bit, 0xff);
}

int bitmask_next_one(struct bitmask *b, size_t from_bit)
{
	return _bitmask_first_one(b, from_bit, 0);
}

static void bitmask_continue_iterator_update(struct bitmask_continue_iterator *it)
{
	it->start = bitmask_next_one(it->b, it->start);
	if (it->start < 0)
		return;

	it->__end = bitmask_next_zero(it->b, it->start + 1);
	if (it->__end < 0) {
		it->bits = it->b->total_bits - it->start;
		return;
	}

	it->bits = it->__end - it->start;
}

struct bitmask_continue_iterator bitmask_continue_iterator_init(struct bitmask *b)
{
	struct bitmask_continue_iterator it = {
		.b = b,
		.start = 0,
	};

	bitmask_continue_iterator_update(&it);

	return it;
}

void bitmask_continue_iterator_next(struct bitmask_continue_iterator *it)
{
	it->start = it->__end;

	if (it->start < 0)
		return;

	bitmask_continue_iterator_update(it);
}

struct bitmask *bitmask_xor(const struct bitmask *a, const struct bitmask *b)
{
	struct bitmask *result;

	if (a->total_bits != b->total_bits)
		return NULL;

	result = alloc_bitmask(a->total_bits);
	if (!result)
		return result;

	for (size_t i = 0; i < b->bufsize; i++)
		result->buffer[i] = a->buffer[i] ^ b->buffer[i];

	return result;
}

