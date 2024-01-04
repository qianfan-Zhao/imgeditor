/*
 * rockchip common header file
 * qianfan Zhao
 */
#ifndef IMGEDITOR_ROCKCHIP_H
#define IMGEDITOR_ROCKCHIP_H

#include <time.h>
#include <stdint.h>

#define RKNONE_DEVICE		0x00
#define RK27_DEVICE		0x10
#define RKCAYMAN_DEVICE		0x11
#define RK28_DEVICE		0x20
#define RK281X_DEVICE		0x21
#define RKPANDA_DEVICE		0x22
#define RKNANO_DEVICE		0x30
#define RKSMART_DEVICE		0x31
#define RKCROWN_DEVICE		0x40
#define RK29_DEVICE		0x50
#define RK292X_DEVICE		0x51
#define RK30_DEVICE		0x60
#define RK30B_DEVICE		0x61
#define RK31_DEVICE		0x70
#define RK32_DEVICE		0x80

struct rkdevice_info {
	uint32_t		device_code;
	const char		*name;
};

/* find rockchip device's informations matched by name or code */
const struct rkdevice_info *get_rkdevice_info(const char *name, uint32_t code);

void structure_item_print_rkdevice_code(const char *print_name_fmt, const char *name,
					const void *addr, size_t sz);

struct __attribute__((packed)) __attribute__((aligned(1))) rktime {
	uint16_t		year;
	uint8_t			mon;
	uint8_t			day;
	uint8_t			hour;
	uint8_t			min;
	uint8_t			sec;
};

void rktime_init(struct rktime *time, time_t t);
void structure_item_print_rktime(const char *print_name_fmt, const char *name,
				 const void *addr, size_t sz);

uint32_t crc32_rk(uint32_t crc, const unsigned char *s, uint32_t len);

#endif
