/*
 * the compatible layer for hash algo
 * qianfan Zhao
 */
#ifndef HASH_COMPATIBLE_H
#define HASH_COMPATIBLE_H

#include <stdio.h>
#include <stdint.h>

enum hash_type_id {
	HASH_TYPE_MD5,
	HASH_TYPE_SHA1,
	HASH_TYPE_SHA224,
	HASH_TYPE_SHA256,
	HASH_TYPE_SHA384,
	HASH_TYPE_SHA512,
};

typedef struct hash_context {
	/* this data is my secret. */
	uint8_t			data[1024];
} hash_context_t;

int hash_context_init(hash_context_t *ctx, enum hash_type_id id);
int hash_context_update(hash_context_t *ctx, const void *data, size_t datalen);
size_t hash_context_finish(hash_context_t *ctx, uint8_t *dgst, size_t bufsize);

#endif
