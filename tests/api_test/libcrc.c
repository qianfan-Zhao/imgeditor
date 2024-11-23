#include "api_test.h"
#include "libcrc.h"

static void crc32_test(enum libcrc32_algo algo, uint32_t result)
{
	struct libcrc32 *crc = libcrc32_predefined(algo);
	uint8_t input[3] = {1, 2, 3};
	uint32_t r;

	libcrc32_init(crc);
	for (size_t i = 0; i < sizeof(input); i++)
		libcrc32_update(crc, &input[i], 1);
	r = libcrc32_finish(crc);

	if (r != result)
		fprintf(stderr, "CRC32 %d failed ", algo);
	assert_xinteq(r, result);
}

static void crc16_test(enum libcrc16_algo algo, uint32_t result)
{
	struct libcrc16 *crc = libcrc16_predefined(algo);
	uint8_t input[3] = {1, 2, 3};
	uint32_t r;

	libcrc16_init(crc);
	for (size_t i = 0; i < sizeof(input); i++)
		libcrc16_update(crc, &input[i], 1);
	r = libcrc16_finish(crc);

	if (r != result)
		fprintf(stderr, "CRC16 %d failed ", algo);
	assert_xinteq(r, result);
}

void crc_test(void)
{
	/* https://www.23bei.com/tool/234.html */
	crc32_test(LIBCRC32_XFER,		0x00AE5FF1);
	crc32_test(LIBCRC32_POSIX,		0x5396EBAE);
	crc32_test(LIBCRC32_BZIP2,		0xE4F296AE);
	crc32_test(LIBCRC32_MPEG_2,		0x1B0D6951);
	crc32_test(LIBCRC32_CRC32,		0x55BC801D);
	crc32_test(LIBCRC32_JAMCRC,		0xAA437FE2);
	crc32_test(LIBCRC32_C,			0xF130F21E);
	crc32_test(LIBCRC32_Q,			0xC371CF00);
	crc32_test(LIBCRC32_D,			0xE92F8EEC);

	/* https://crccalc.com/?crc=010203&method=CRC-16&datatype=1&outtype=0 */
	crc16_test(LIBCRC16_CCITT,		0x5BF7);
	crc16_test(LIBCRC16_CCITT_FALSE,	0xADAD);
	crc16_test(LIBCRC16_XMODEM,		0x6131);
	crc16_test(LIBCRC16_X25,		0x9D3B);
	crc16_test(LIBCRC16_AUG_CCITT,		0x703D);
	crc16_test(LIBCRC16_MODBUS,		0x6161);
	crc16_test(LIBCRC16_IBM,		0xA110);
	crc16_test(LIBCRC16_MAXIM,		0x5EEF);
	crc16_test(LIBCRC16_USB,		0x9E9E);
	crc16_test(LIBCRC16_DNP,		0xA740);
}
