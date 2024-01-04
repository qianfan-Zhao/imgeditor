/*
 * common function for rockchip devices
 * qianfan Zhao
 */
#include <string.h>
#include "imgeditor.h"
#include "rockchip.h"
#include "structure.h"

static const struct rkdevice_info available_rkdevices[] = {
	{ RKNONE_DEVICE,	"RKNONE"	},
	{ RK27_DEVICE,		"RK27"		},
	{ RKCAYMAN_DEVICE,	"RKCAYMAN"	},
	{ RK28_DEVICE,		"RK28"		},
	{ RK281X_DEVICE,	"RK281X"	},
	{ RKPANDA_DEVICE,	"RKPANDA"	},
	{ RKNANO_DEVICE,	"RKNANO"	},
	{ RKSMART_DEVICE,	"RKSMART"	},
	{ RKCROWN_DEVICE,	"RKCROWN"	},
	{ RK29_DEVICE,		"RK29"		},
	{ RK292X_DEVICE,	"RK292X"	},
	{ RK30_DEVICE,		"RK30"		},
	{ RK30B_DEVICE,		"RK30B"		},
	{ RK31_DEVICE,		"RK31"		},
	{ RK32_DEVICE,		"RK32"		},
	{ 0x33353638,		"RK3568"	},
	{ 0,			NULL		},
};

/* find matched rkdevice by name or device code. */
const struct rkdevice_info *get_rkdevice_info(const char *name, uint32_t code)
{
	const struct rkdevice_info *info;

	for (int i = 0; available_rkdevices[i].name; i++) {
		info = &available_rkdevices[i];

		if (name) {
			if (!strcmp(name, info->name))
				return info;
		} else if (code == info->device_code) {
			return info;
		}
	}

	return NULL;
}

void structure_item_print_rkdevice_code(const char *print_name_fmt, const char *name,
					const void *addr, size_t sz)
{
	uint32_t device_code = *((uint32_t *)addr);
	const struct rkdevice_info *info;

	structure_print_name(print_name_fmt, name);
	info = get_rkdevice_info(NULL, device_code);
	if (info)
		printf("%s\n", info->name);
	else
		printf("0x%08x\n", device_code);

}

void rktime_init(struct rktime *time, time_t t)
{
	struct tm *local = localtime(&t);

	time->year = local->tm_year + 1900;
	time->mon = local->tm_mon + 1;
	time->day = local->tm_mday;
	time->hour = local->tm_hour;
	time->min = local->tm_min;
	time->sec = local->tm_sec;
}

void structure_item_print_rktime(const char *print_name_fmt, const char *name,
				 const void *addr, size_t sz)
{
	const struct rktime *time = addr;

	structure_print_name(print_name_fmt, name);
	printf("%04d-%02d-%02d %02d:%02d:%02d\n",
		time->year, time->mon, time->day,
		time->hour, time->min, time->sec);
}
