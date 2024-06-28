/*
 * the compatible layer for hash algo
 * qianfan Zhao
 */
#ifndef HASH_COMPATIBLE_H
#define HASH_COMPATIBLE_H

#define OPENSSL_NO_DEPRECATED
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#define MD5_DIGEST_SIZE		16
#define SHA1_DIGEST_SIZE	20
#define SHA256_DIGEST_SIZE	32
#define SHA512_DIGEST_SIZE	64

typedef struct { EVP_MD_CTX *ctx; } MD5_CTX_t;
typedef struct { EVP_MD_CTX *ctx; } SHA1_CTX_t;
typedef struct { EVP_MD_CTX *ctx; } SHA256_CTX_t;
typedef struct { EVP_MD_CTX *ctx; } SHA512_CTX_t;

MD5_CTX_t *md5_new(void);
void md5_update(MD5_CTX_t *ctx, const void *data, size_t datalen);
void md5_finish(MD5_CTX_t *ctx, uint8_t *dgst);
void md5_free(MD5_CTX_t *ctx);

SHA1_CTX_t *sha1_new(void);
void sha1_update(SHA1_CTX_t *ctx, const void *data, size_t datalen);
void sha1_finish(SHA1_CTX_t *ctx, uint8_t *dgst);
void sha1_free(SHA1_CTX_t *ctx);

SHA256_CTX_t *sha256_new(void);
void sha256_update(SHA256_CTX_t *ctx, const void *data, size_t datalen);
void sha256_finish(SHA256_CTX_t *ctx, uint8_t *dgst);
void sha256_free(SHA256_CTX_t *ctx);

SHA512_CTX_t *sha512_new(void);
void sha512_update(SHA512_CTX_t *ctx, const void *data, size_t datalen);
void sha512_finish(SHA512_CTX_t *ctx, uint8_t *dgst);
void sha512_free(SHA512_CTX_t *ctx);

#endif
