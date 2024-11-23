/*
 * crc helper library
 * qianfan Zhao
 */
#include <assert.h>
#include "libcrc.h"

#define define_libcrc32_algo(_algo, _poly, _seed, _xor, _refin, _refout) \
[_algo] = {				\
	.poly		= _poly,	\
	.xor_result	= _xor,		\
	.seed		= _seed,	\
	.refin		= _refin,	\
	.refout		= _refout,	\
}

static struct libcrc32 available_crc32_algos[LIBCRC32_MAX] = {
	define_libcrc32_algo(LIBCRC32_XFER,	0x000000AF, 0x00000000, 0x00000000, false, false),
	define_libcrc32_algo(LIBCRC32_POSIX,	0x04C11DB7, 0x00000000, 0xFFFFFFFF, false, false),
	define_libcrc32_algo(LIBCRC32_BZIP2,	0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, false, false),
	define_libcrc32_algo(LIBCRC32_MPEG_2,	0x04C11DB7, 0xFFFFFFFF, 0x00000000, false, false),
	define_libcrc32_algo(LIBCRC32_CRC32,	0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true,  true),
	define_libcrc32_algo(LIBCRC32_JAMCRC,	0x04C11DB7, 0xFFFFFFFF, 0x00000000, true,  true),
	define_libcrc32_algo(LIBCRC32_C,	0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true,  true),
	define_libcrc32_algo(LIBCRC32_Q,	0x814141AB, 0x00000000, 0x00000000, false, false),
	define_libcrc32_algo(LIBCRC32_D,	0xA833982B, 0xFFFFFFFF, 0xFFFFFFFF, true,  true),
};

struct libcrc32 *libcrc32_predefined(enum libcrc32_algo algo)
{
	if (algo < 0 || algo >= LIBCRC32_MAX)
		return NULL;

	return &available_crc32_algos[algo];
}

#define CRC_TABLE_SIZE				256

struct libcrc32_private_data {
	bool					crc_table_generated;
	uint32_t				next;
	uint32_t				crc_table[256];
};

static struct libcrc32_private_data *
			libcrc32_get_private_data(struct libcrc32 *crc)
{
	if (!crc)
		return NULL;

	static_assert(
		sizeof(crc->private_data) >= sizeof(struct libcrc32_private_data),
		"libcrc32.private data size limit");

	return (struct libcrc32_private_data *)crc->private_data;
}

static void libcrc32_generate_table(struct libcrc32 *crc)
{
	struct libcrc32_private_data *p = libcrc32_get_private_data(crc);

	if (!p)
		return;

	for (uint32_t i = 0; i < CRC_TABLE_SIZE; i++) {
		uint32_t cur_byte = crc->refin ? (reverse32(i) >> 24) : i;

		for (int bit = 0; bit < 32; bit++) {
			if ((cur_byte & 0x80000000))
				cur_byte = (cur_byte << 1) ^ crc->poly;
			else
				cur_byte <<= 1;
		}

		p->crc_table[i] = crc->refout ? reverse32(cur_byte) : cur_byte;
	}
}

int libcrc32_init_seed(struct libcrc32 *crc, uint32_t seed)
{
	struct libcrc32_private_data *p = libcrc32_get_private_data(crc);

	if (!p)
		return -1;

	if (!p->crc_table_generated) {
		p->crc_table_generated = true;
		libcrc32_generate_table(crc);
	}

	p->next = seed;
	return 0;
}

int libcrc32_init(struct libcrc32 *crc)
{
	if (!crc)
		return -1;

	return libcrc32_init_seed(crc, crc->seed);
}

int libcrc32_update(struct libcrc32 *crc, const void *data, size_t len)
{
	struct libcrc32_private_data *p = libcrc32_get_private_data(crc);
	const uint8_t *p_data = data;

	if (!p)
		return -1;

	if (crc->refout) {
		for (size_t i = 0; i < len; i++)
			p->next = (p->next >> 8) ^ p->crc_table[(p->next ^ p_data[i]) & 0xff];
	} else {
		for (size_t i = 0; i < len; i++)
			p->next = (p->next << 8) ^ p->crc_table[((p->next >> 24) ^ p_data[i]) & 0xff];
	}

	return 0;
}

uint32_t libcrc32_finish(struct libcrc32 *crc)
{
	struct libcrc32_private_data *p = libcrc32_get_private_data(crc);

	if (!crc)
		return (uint32_t)-1;

	return p->next ^ crc->xor_result;
}

#define define_libcrc16_algo(_algo, _poly, _seed, _xor, _refin, _refout) \
[_algo] = {				\
	.poly		= _poly,	\
	.xor_result	= _xor,		\
	.seed		= _seed,	\
	.refin		= _refin,	\
	.refout		= _refout,	\
}

static struct libcrc16 available_crc16_algos[LIBCRC16_MAX] = {
	define_libcrc16_algo(LIBCRC16_CCITT,		0x1021, 0x0000, 0x0000, true,	true),
	define_libcrc16_algo(LIBCRC16_CCITT_FALSE,	0x1021, 0xffff, 0x0000, false,	false),
	define_libcrc16_algo(LIBCRC16_XMODEM,		0x1021, 0x0000, 0x0000, false,	false),
	define_libcrc16_algo(LIBCRC16_X25,		0x1021, 0xffff, 0xffff, true,	true),
	define_libcrc16_algo(LIBCRC16_AUG_CCITT,	0x1021, 0x1d0f, 0x0000, false,	false),
	define_libcrc16_algo(LIBCRC16_MODBUS,		0x8005, 0xffff, 0x0000, true,	true),
	define_libcrc16_algo(LIBCRC16_IBM,		0x8005, 0x0000, 0x0000, true,	true),
	define_libcrc16_algo(LIBCRC16_MAXIM,		0x8005, 0x0000, 0xffff, true,	true),
	define_libcrc16_algo(LIBCRC16_USB,		0x8005, 0xffff, 0xffff, true,	true),
	define_libcrc16_algo(LIBCRC16_DNP,		0x3d65, 0x0000, 0xffff, true,	true),
};

struct libcrc16 *libcrc16_predefined(enum libcrc16_algo algo)
{
	if (algo < 0 || algo >= LIBCRC16_MAX)
		return NULL;

	return &available_crc16_algos[algo];
}

struct libcrc16_private_data {
	bool					crc_table_generated;
	uint16_t				next;
	uint16_t				crc_table[256];
};

static struct libcrc16_private_data *
			libcrc16_get_private_data(struct libcrc16 *crc)
{
	if (!crc)
		return NULL;

	static_assert(
		sizeof(crc->private_data) >= sizeof(struct libcrc16_private_data),
		"libcrc16.private data size limit");

	return (struct libcrc16_private_data *)crc->private_data;
}

static void libcrc16_generate_table(struct libcrc16 *crc)
{
	struct libcrc16_private_data *p = libcrc16_get_private_data(crc);

	if (!p)
		return;

	for (uint32_t i = 0; i < CRC_TABLE_SIZE; i++) {
		uint16_t n = crc->refin ? reverse16(i) >> 8 : i;

		for (int bit = 0; bit < 16; bit++) {
			if ((n & 0x8000))
				n = (n << 1) ^ crc->poly;
			else
				n <<= 1;
		}

		p->crc_table[i] = crc->refout ? reverse16(n) : n;
	}
}

int libcrc16_init_seed(struct libcrc16 *crc, uint16_t seed)
{
	struct libcrc16_private_data *p = libcrc16_get_private_data(crc);

	if (!p)
		return -1;

	if (!p->crc_table_generated) {
		p->crc_table_generated = true;
		libcrc16_generate_table(crc);
	}

	p->next = seed;
	return 0;
}

int libcrc16_init(struct libcrc16 *crc)
{
	if (!crc)
		return -1;

	return libcrc16_init_seed(crc, crc->seed);
}

int libcrc16_update(struct libcrc16 *crc, const void *data, size_t len)
{
	struct libcrc16_private_data *p = libcrc16_get_private_data(crc);
	const uint8_t *p_data = data;

	if (!p)
		return -1;

	if (crc->refout) {
		for (size_t i = 0; i < len; i++)
			p->next = (p->next >> 8) ^ p->crc_table[(p->next ^ p_data[i]) & 0xff];
	} else {
		for (size_t i = 0; i < len; i++)
			p->next = (p->next << 8) ^ p->crc_table[((p->next >> 8) ^ p_data[i]) & 0xff];
	}

	return 0;
}

uint16_t libcrc16_finish(struct libcrc16 *crc)
{
	struct libcrc16_private_data *p = libcrc16_get_private_data(crc);

	if (!crc)
		return (uint16_t)-1;

	return p->next ^ crc->xor_result;
}
