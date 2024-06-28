#include "hash_compatible.h"
#include <openssl/evp.h>

#define DEFINE_HASH_ALGO(name, evpname, type)				\
type * name##_new(void)							\
{									\
	type *c = malloc(sizeof(type));					\
	if (!c)								\
		return c;						\
	c->ctx = EVP_MD_CTX_new();					\
	EVP_DigestInit_ex(c->ctx, EVP_get_digestbyname(evpname), NULL);	\
	return c;							\
}									\
									\
void name##_free(type *c)						\
{									\
	EVP_MD_CTX_free(c->ctx);					\
	free(c);							\
}									\
									\
void name##_update(type *c, const void *data, size_t datalen)	\
{									\
	EVP_DigestUpdate(c->ctx, data, datalen);			\
}									\
									\
void name##_finish(type *c, uint8_t *dgst)				\
{									\
	EVP_DigestFinal(c->ctx, dgst, NULL);				\
}									\

DEFINE_HASH_ALGO(md5, "MD5", MD5_CTX_t);
DEFINE_HASH_ALGO(sha1, "SHA1", SHA1_CTX_t);
DEFINE_HASH_ALGO(sha256, "SHA256", SHA256_CTX_t);
DEFINE_HASH_ALGO(sha512, "SHA512", SHA512_CTX_t);
