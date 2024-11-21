/*
 * common hash interface
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <string.h>
#include <assert.h>
#include "hash_compatible.h"

#include "rfc1321/global.h"
#include "rfc1321/md5.h"

#include "rfc6234/sha.h"

struct hash_context_private {
	enum hash_type_id		id;

	uint8_t				context[512];
	uint8_t				dgst[1024 / 8];
};

static inline void *context(struct hash_context_private *p)
{
	return (void *)p->context;
}

int hash_context_init(hash_context_t *ctx, enum hash_type_id id)
{
	struct hash_context_private *p = (struct hash_context_private *)ctx;

	static_assert(sizeof(*p) <= sizeof(*ctx), "CTX");
	static_assert(sizeof(p->context) >= sizeof(SHA1Context), "SHA1");
	static_assert(sizeof(p->context) >= sizeof(SHA256Context), "SHA256");
	static_assert(sizeof(p->context) >= sizeof(SHA512Context), "SHA512");

	static_assert(sizeof(p->dgst) >= SHA512HashSize, "SHA512HashSize");

	memset(ctx, 0, sizeof(*ctx));
	p->id = id;

#define CASE_SHA(type) case HASH_TYPE_##type: type##Reset(context(p)); break
	switch (id) {
	case HASH_TYPE_MD5:
		MD5Init(context(p));
		break;
	CASE_SHA(SHA1);
	CASE_SHA(SHA224);
	CASE_SHA(SHA256);
	CASE_SHA(SHA384);
	CASE_SHA(SHA512);
	default:
		return -1;
	}
#undef CASE_SHA

	return 0;
}

int hash_context_update(hash_context_t *ctx, const void *data, size_t datalen)
{
	struct hash_context_private *p = (struct hash_context_private *)ctx;
	int ret = -1;

#define CASE_SHA(type) case HASH_TYPE_##type: 			\
	ret = - type##Input(context(p), data, datalen);		\
	break

	switch (p->id) {
	case HASH_TYPE_MD5:
		MD5Update(context(p), data, datalen);
		ret = 0;
		break;
	CASE_SHA(SHA1);
	CASE_SHA(SHA224);
	CASE_SHA(SHA256);
	CASE_SHA(SHA384);
	CASE_SHA(SHA512);
	}
#undef CASE_SHA

	return ret;
}

size_t hash_context_finish(hash_context_t *ctx, uint8_t *dgst, size_t bufsz)
{
	struct hash_context_private *p = (struct hash_context_private *)ctx;
	size_t dgst_sz = 0;
	int ret = 0;

#define CASE_SHA(type) case HASH_TYPE_##type: 			\
	ret = - type##Result(context(p), p->dgst);		\
	dgst_sz = type##HashSize;				\
	break

	switch (p->id) {
	case HASH_TYPE_MD5:
		MD5Final(p->dgst, context(p));
		dgst_sz = 16;
		break;
	CASE_SHA(SHA1);
	CASE_SHA(SHA224);
	CASE_SHA(SHA256);
	CASE_SHA(SHA384);
	CASE_SHA(SHA512);
	}
#undef CASE_SHA

	if (dgst_sz == 0 || ret < 0) /* falied */
		return 0;

	if (dgst_sz > bufsz)
		dgst_sz = bufsz;
	memcpy(dgst, p->dgst, dgst_sz);

	return dgst_sz;
}
