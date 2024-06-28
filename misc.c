/*
 * misc functions
 */
#include "imgeditor.h"

const char *smart_format_size(uint64_t byte_addr, char *buf, size_t bufsz)
{
	uint64_t addr;
	const char *units;

	if (byte_addr <= SIZE_KB(1) || (byte_addr & SIZEMASK_KB)) {
		addr = byte_addr;
		units = "B  ";
	} else if (byte_addr & SIZEMASK_MB) {
		addr = byte_addr / SIZE_KB(1);
		units = "KiB";
	} else if (byte_addr & SIZEMASK_GB) {
		addr = byte_addr / SIZE_MB(1);
		units = "MiB";
	} else {
		addr = byte_addr / SIZE_GB(1);
		units = "GiB";
	}

	snprintf(buf, bufsz, "%" PRIu64 "%s", addr, units);
	return buf;
}
