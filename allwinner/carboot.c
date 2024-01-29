/*
 * Allwinner CarBoot image editor
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include "imgeditor.h"
#include "structure.h"
#include "string_helper.h"

#define STAMP_VALUE		0x5F0A6C39

struct user_gpio_set {
	char			name[32];
	int			port;
	int			port_num;
	int			mul;
	int			pull;
	int			driver;
	int			data;
};

static const uint8_t carboot_magic[8] = "CarBoot";

struct __attribute__((packed)) carboot_head {
	struct user_gpio_set	reverse_io;
	char			magic[8];	/* CarBoot\0 */
	__le32			sum;
	__le32			length;
	__le32			kernel_start;
	__le32			kernel_part_start;
	__le32			kernel_part_sz;
	__le32			fdt_start;
	__le32			start_verify;
	__le32			fdt_info_sz;
	__le32			car_cfg_sz;
	uint8_t			used;

	/* the next is fdt and car_cfg */
	uint8_t			fdt[0];
};

static int snprintf_gpio_property(char **s, size_t *sz, __le32 prop)
{
	int n = le32_to_cpu(prop);

	if (n < 0)
		snprintf_continue(s, sz, "<default>");
	else
		snprintf_continue(s, sz, "<%d>", n);

	return 0;
}

static int snprintf_user_gpio_set(const struct user_gpio_set *gpio, char *buf,
				  size_t sz)
{
	char *s = buf;

	snprintf_continue(&s, &sz, "port:");
	snprintf_continue(&s, &sz, "P%c%02d",
			  'A' + le32_to_cpu(gpio->port) - 1,
			  le32_to_cpu(gpio->port_num));

	snprintf_gpio_property(&s, &sz, gpio->mul);
	snprintf_gpio_property(&s, &sz, gpio->pull);
	snprintf_gpio_property(&s, &sz, gpio->driver);
	snprintf_gpio_property(&s, &sz, gpio->data);

	return 0;
}

static void structure_item_print_user_gpio_set(const char *print_name_fmt,
					       const char *name,
					       const void *addr, size_t sz)
{
	const struct user_gpio_set *gpio = addr;
	char s[128];

	structure_print_name(print_name_fmt, name);

	snprintf_user_gpio_set(gpio, s, sizeof(s));
	printf("%s = %s\n", gpio->name, s);
}

static int structure_item_save_user_gpio_set(cJSON *root, const char *name,
					     const void *addr, size_t sz)
{
	cJSON *json = cJSON_AddObjectToObject(root, name);
	const struct user_gpio_set *gpio = addr;
	char s[128];

	if (!json)
		return -1;

	snprintf_user_gpio_set(gpio, s, sizeof(s));
	json_create_or_update_string_value(json, "name", gpio->name);
	json_create_or_update_string_value(json, "gpio", s);

	return 0;
}

static const struct structure_item structure_carboot_head[] = {
	STRUCTURE_ITEM(struct carboot_head,	reverse_io,		structure_item_print_user_gpio_set,
			.save_json = structure_item_save_user_gpio_set),
	STRUCTURE_ITEM(struct carboot_head,	length,			structure_item_print_unsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct carboot_head,	kernel_start,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct carboot_head,	kernel_part_start,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct carboot_head,	kernel_part_sz,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct carboot_head,	fdt_start,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct carboot_head,	start_verify,		structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct carboot_head,	fdt_info_sz,		structure_item_print_unsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct carboot_head,	car_cfg_sz,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct carboot_head,	used,			structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM_END(),
};

struct carboot_private_data {
	struct carboot_head	head;
};

static void sunxi_stamp_update_checksum(uint8_t *buf, size_t buster_sz,
					void *arg)
{
	uint32_t *sum = arg, *buf32 = (uint32_t *)buf;
	size_t i = 0;

	for (; i < buster_sz / sizeof(*buf32); i++)
		(*sum) += buf32[i];
}

static int carboot_detect(void *private_data, int force_type, int fd)
{
	struct carboot_private_data *p = private_data;
	uint32_t sum = STAMP_VALUE;
	int ret;

	ret = fileread(fd, &p->head, sizeof(p->head));
	if (ret < 0)
		return ret;

	if (le32_to_cpu(p->head.length) > SIZE_MB(4)) {
		fprintf_if_force_type("Error: invalid length\n");
		return -1;
	}

	if (memcmp(p->head.magic, carboot_magic, sizeof(carboot_magic))) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	dd(fd, -1, 0, -1, le32_to_cpu(p->head.length),
	   sunxi_stamp_update_checksum, &sum);

	sum -= le32_to_cpu(p->head.sum);
	if (sum != le32_to_cpu(p->head.sum)) {
		fprintf_if_force_type("Error: checksum doesn't match\n");
		return -1;
	}

	return 0;
}

static int carboot_list(void *private_data, int fd, int argc, char **argv)
{
	struct carboot_private_data *p = private_data;

	structure_print("%-20s", &p->head, structure_carboot_head);
	return 0;
}

static int carboot_unpack(void *private_data, int fd, const char *outdir,
			  int argc, char **argv)
{
	struct carboot_private_data *p = private_data;
	uint32_t fdt_sz = le32_to_cpu(p->head.fdt_info_sz);
	cJSON *root = cJSON_CreateObject();
	char filename[1024];
	int fd_out, ret = -1;

	if (!root)
		return ret;

	ret = structure_save_json(root, &p->head, structure_carboot_head);
	if (ret < 0)
		goto done;

	snprintf(filename, sizeof(filename), "%s/carboot.json", outdir);
	json_saveto_file(root, filename);

	/* save fdt */
	snprintf(filename, sizeof(filename), "%s/fdt.dtb", outdir);
	fd_out = fileopen(filename, O_RDWR | O_CREAT, 0664);
	if (fd_out < 0) {
		ret = fd_out;
		goto done;
	}
	dd(fd, fd_out, sizeof(struct carboot_head), 0, fdt_sz, NULL, NULL);
	close(fd_out);

	/* save car_cfg */
	if (le32_to_cpu(p->head.car_cfg_sz) > 0) {
		snprintf(filename, sizeof(filename), "%s/car.cfg", outdir);
		fd_out = fileopen(filename, O_RDWR | O_CREAT, 0664);
		if (fd_out < 0) {
			ret = fd_out;
			goto done;
		}
		dd(fd, fd_out, sizeof(struct carboot_head) + fdt_sz, 0,
			le32_to_cpu(p->head.car_cfg_sz),
			NULL, NULL);
		close(fd_out);
	}

done:
	cJSON_Delete(root);
	return ret;
}

static struct imgeditor sunxi_carboot = {
	.name			= "sunxi-carboot",
	.descriptor		= "allwinner CarBoot image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct carboot_head),
	.private_data_size	= sizeof(struct carboot_private_data),
	.detect			= carboot_detect,
	.list			= carboot_list,
	.unpack			= carboot_unpack,

	.search_magic		= {
		.magic		= carboot_magic,
		.magic_sz	= sizeof(carboot_magic),
		.magic_offset	= offsetof(struct carboot_head, magic),
	}
};
REGISTER_IMGEDITOR(sunxi_carboot);
