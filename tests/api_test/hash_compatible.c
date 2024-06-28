#include "api_test.h"
#include "hash_compatible.h"

static size_t bufpushc(char **p_buf, size_t *p_sz, char c)
{
	char *buf = *p_buf;
	size_t n = 0;

	if (*p_sz >= 1) {
		*buf = c;
		n = 1;
	}

	(*p_buf) = buf + n;
	(*p_sz) -= n;

	return n;
}

static char hexc(int c)
{
	static const char hexc_tables[] = "0123456789abcdef";

	return hexc_tables[c & 0xf];
}

static const char *hexstring(char *buf, size_t bufsz, const void *data,
			     size_t data_len)
{
	const char *start = buf;
	bufsz -= 1; /* for null */

	for (size_t i = 0; i < data_len; i++) {
		uint8_t n = ((const uint8_t *)data)[i];

		bufpushc(&buf, &bufsz, hexc((n >> 4) & 0xf));
		bufpushc(&buf, &bufsz, hexc((n >> 0) & 0xf));
	}

	bufsz += 1;
	bufpushc(&buf, &bufsz, 0);

	return start;
}

#define DEFINE_HASH_ALGO_TEST(name, type, dgstsz, result)		\
static void hash_algo_##name##_test(void)				\
{									\
	char s[1024];							\
	uint8_t sum[dgstsz] = { 0 };					\
	type *c;							\
									\
	c = name##_new();						\
	name##_update(c, "a", 1);					\
	name##_update(c, "b", 1);					\
	name##_update(c, "c", 1);					\
									\
	name##_finish(c, sum);						\
	name##_free(c);							\
									\
	hexstring(s, sizeof(s), sum, dgstsz);				\
	assert_streq(s, result);					\
}

DEFINE_HASH_ALGO_TEST(md5, MD5_CTX_t, MD5_DIGEST_SIZE,
"900150983cd24fb0d6963f7d28e17f72");
DEFINE_HASH_ALGO_TEST(sha1, SHA1_CTX_t, SHA1_DIGEST_SIZE,
"a9993e364706816aba3e25717850c26c9cd0d89d");
DEFINE_HASH_ALGO_TEST(sha256, SHA256_CTX_t, SHA256_DIGEST_SIZE,
"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
DEFINE_HASH_ALGO_TEST(sha512, SHA512_CTX_t, SHA512_DIGEST_SIZE,
"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");

void hash_compatible_test(void)
{
	hash_algo_md5_test();
	hash_algo_sha1_test();
	hash_algo_sha256_test();
	hash_algo_sha512_test();
}
