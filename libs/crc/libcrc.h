/*
 * common crc library
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef LIBCRC_H
#define LIBCRC_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static inline uint8_t reverse8(uint8_t x)
{
	x = (((x & 0xaa) >> 1) | ((x & 0x55) << 1));
	x = (((x & 0xcc) >> 2) | ((x & 0x33) << 2));
	x = (((x & 0xf0) >> 4) | ((x & 0x0f) << 4));

	return x;
}

static inline uint16_t reverse16(uint16_t x)
{
	x = (((x & 0xaaaa) >> 1) | ((x & 0x5555) << 1));
	x = (((x & 0xcccc) >> 2) | ((x & 0x3333) << 2));
	x = (((x & 0xf0f0) >> 4) | ((x & 0x0f0f) << 4));
	x = (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));

	return x;
}

static inline uint32_t reverse32(uint32_t x)
{
	x = ((x & 0x55555555) <<  1) | ((x >>  1) & 0x55555555);
	x = ((x & 0x33333333) <<  2) | ((x >>  2) & 0x33333333);
	x = ((x & 0x0F0F0F0F) <<  4) | ((x >>  4) & 0x0F0F0F0F);
	x = (x << 24) | ((x & 0xFF00) << 8) |
		((x >> 8) & 0xFF00) | (x >> 24);

	return x;
}

struct libcrc32 {
	uint32_t		poly;
	uint32_t		seed;
	uint32_t		xor_result;
	bool			refin;
	bool			refout;

	uint8_t			private_data[2048];
};

enum libcrc32_algo {
	LIBCRC32_XFER,
	LIBCRC32_POSIX,
	LIBCRC32_BZIP2,
	LIBCRC32_MPEG_2,
	LIBCRC32_CRC32,
	LIBCRC32_JAMCRC,
	LIBCRC32_C,
	LIBCRC32_Q,
	LIBCRC32_D,
	LIBCRC32_MAX,
};

struct libcrc32 *libcrc32_predefined(enum libcrc32_algo algo);

/* use the default seed */
int libcrc32_init(struct libcrc32 *crc);
int libcrc32_update(struct libcrc32 *crc, const void *data, size_t len);
uint32_t libcrc32_finish(struct libcrc32 *crc);

/* use this seed once */
int libcrc32_init_seed(struct libcrc32 *crc, uint32_t seed);

struct libcrc16 {
	uint16_t		poly;
	uint16_t		seed;
	uint16_t		xor_result;
	bool			refin;
	bool			refout;
	uint8_t			private_data[1024];
};

enum libcrc16_algo {
	LIBCRC16_CCITT,
	LIBCRC16_CCITT_FALSE,
	LIBCRC16_XMODEM,
	LIBCRC16_X25,
	LIBCRC16_AUG_CCITT,
	LIBCRC16_MODBUS,
	LIBCRC16_IBM,
	LIBCRC16_MAXIM,
	LIBCRC16_USB,
	LIBCRC16_DNP,
	LIBCRC16_MAX,
};

struct libcrc16 *libcrc16_predefined(enum libcrc16_algo algo);

int libcrc16_init(struct libcrc16 *crc);
int libcrc16_update(struct libcrc16 *crc, const void *data, size_t len);
uint16_t libcrc16_finish(struct libcrc16 *crc);

int libcrc16_init_seed(struct libcrc16 *crc, uint16_t seed);

#endif
