/*
 * there has so many old code use this crc32 function. let's make a compatible.
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include "imgeditor.h"
#include "libcrc.h"

uint32_t crc32(uint32_t crc, const uint8_t *p, size_t len)
{
	struct libcrc32 *algo = libcrc32_predefined(LIBCRC32_CRC32);

	libcrc32_init_seed(algo, crc ^ 0xffffffff);
	libcrc32_update(algo, p, len);

	return libcrc32_finish(algo);
}
