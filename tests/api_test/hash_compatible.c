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
	hash_context_t hash;						\
	uint8_t sum[128] = { 0 };					\
	size_t hashsz;							\
									\
	hash_context_init(&hash, type);					\
	hash_context_update(&hash, "a", 1);				\
	hash_context_update(&hash, "b", 1);				\
	hash_context_update(&hash, "c", 1);				\
									\
	hashsz = hash_context_finish(&hash, sum, sizeof(sum));		\
									\
	assert_inteq((int)hashsz, dgstsz);				\
	hexstring(s, sizeof(s), sum, hashsz);				\
	assert_streq(s, result);					\
}

DEFINE_HASH_ALGO_TEST(md5, HASH_TYPE_MD5, 16,
"900150983cd24fb0d6963f7d28e17f72");
DEFINE_HASH_ALGO_TEST(sha1, HASH_TYPE_SHA1, 20,
"a9993e364706816aba3e25717850c26c9cd0d89d");
DEFINE_HASH_ALGO_TEST(sha224, HASH_TYPE_SHA224, 28,
"23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");
DEFINE_HASH_ALGO_TEST(sha256, HASH_TYPE_SHA256, 32,
"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
DEFINE_HASH_ALGO_TEST(sha384, HASH_TYPE_SHA384, 48,
"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
DEFINE_HASH_ALGO_TEST(sha512, HASH_TYPE_SHA512, 64,
"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");

void hash_compatible_test(void)
{
	hash_algo_md5_test();
	hash_algo_sha1_test();
	hash_algo_sha224_test();
	hash_algo_sha256_test();
	hash_algo_sha384_test();
	hash_algo_sha512_test();
}
