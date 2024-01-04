/*
 * allwinner bsp eGON header for boot0
 * qianfan Zhao 2022-12-27
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "imgeditor.h"
#include "structure.h"
#include "string_helper.h"

#define PRINT_LEVEL0			"%-20s: "
#define PRINT_LEVEL1			"    %-16s: "
#define PRINT_LEVEL3			"        %-12s: "

#define BOOT0_MAGIC			"eGON.BT0"
#define STAMP_VALUE			0x5F0A6C39

/* eGON binary layout:
 *
 * 1. based on u-boot-2014.07/sunxi_spl/boot0/boot0.lds
 * .head: { main/boot0_head.o (.rodata) }
 * .hash: { main/boot0_hash.o (.data) }
 * .text
 * .rodata
 * .data
 * .bss
 *
 * 2. based on u-boot-2014.07/tools/gen_check_sum
 * gen_check_sum will padding it with random().
 */

struct boot_header {
	__le32				jump_instruction;
	uint8_t				magic[8];
	__le32				checksum;
	__le32				length;
	__le32				this_struct_size;
	__le32				version;
	__le32				return_addr;
	__le32				go_addr;
	__le32				boot_cpu;
	uint8_t				platform[8];
};

/* ref: u-boot-2014.07/sunxi_spl/boot0/main/boot_head.c */
static uint32_t get_boot_bin_offset(struct boot_header *bh)
{
	uint32_t offset;

	offset = le32_to_cpu(bh->jump_instruction) & 0x00ffffff;
	offset += 2;
	offset *= sizeof(uint32_t);

	return offset;
}

static const struct structure_item structure_boot_header[] = {
	STRUCTURE_ITEM(struct boot_header, jump_instruction,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct boot_header, length,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct boot_header, version,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct boot_header, return_addr,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct boot_header, go_addr,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct boot_header, boot_cpu,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct boot_header, platform,		structure_item_print_x8_array),
	STRUCTURE_ITEM_END(),
};

struct lichee_gpio_cfg {
	/* port: 1 = PORTA, 2 = PORTB ... */
	uint8_t				port;
	uint8_t				port_num;
	uint8_t				mul;
	uint8_t				pull;
	uint8_t				driver;
	uint8_t				data;
	uint8_t				reserved[2];
};

static int lichee_gpio_cfg_port_to_char(uint8_t port)
{
	return port + 'A' - 1;
}

static int char_to_lichee_gpio_cfg_port(int ch)
{
	return ch - 'A' + 1;
}

struct lichee_dram_para {
	__le32				dram_clk;
	__le32				dram_type;
	__le32				dram_zq;
	__le32				dram_odt_en;
	__le32				dram_para1;
	__le32				dram_para2;
	__le32				dram_mr0;
	__le32				dram_mr1;
	__le32				dram_mr2;
	__le32				dram_mr3;
	__le32				dram_tpr0;
	__le32				dram_tpr1;
	__le32				dram_tpr2;
	__le32				dram_tpr3;
	__le32				dram_tpr4;
	__le32				dram_tpr5;
	__le32				dram_tpr6;
	__le32				dram_tpr7;
	__le32				dram_tpr8;
	__le32				dram_tpr9;
	__le32				dram_tpr10;
	__le32				dram_tpr11;
	__le32				dram_tpr12;
	__le32				dram_tpr13;
	__le32				reserved[8];
};

static const struct structure_item structure_lichee_dram_para[] = {
	STRUCTURE_ITEM(struct lichee_dram_para, dram_clk,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_type,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_zq,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_odt_en,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_para1,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_para2,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_mr0,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_mr1,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_mr2,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_mr3,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr0,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr1,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr2,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr3,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr4,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr5,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr6,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr7,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr8,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr9,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr10,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr11,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr12,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_dram_para, dram_tpr13,	structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

/* allwinner lichee bsp private head */
struct lichee_private_head {
	__le32				this_struct_size; /* always 0 */
	__le32				debug_mode;
	struct lichee_dram_para		dram_para;
	__le32				uart_port;
	struct lichee_gpio_cfg		uart_gpios[2];
	__le32				enable_jtag;
	struct lichee_gpio_cfg		jtag_gpios[5];
	struct lichee_gpio_cfg		storage_gpios[32]; /* storage media gpios, such as mmc or nand */
	uint8_t				storage_data[512 - sizeof(struct lichee_gpio_cfg) * 32];
};

static void print_lichee_gpio_cfg_arrays(const char *print_name_fmt,
					 const char *name,
					 const void *addr, size_t sz)
{
	size_t gpio_sz = sizeof(struct lichee_gpio_cfg);
	const struct lichee_gpio_cfg *cfgs = addr;
	int print_level = 0;

	assert((sz % gpio_sz) == 0);
	structure_print_name(print_name_fmt, name); /* no newline here */

	for (size_t i = 0; i < sz / gpio_sz; i++) {
		const struct lichee_gpio_cfg *cfg = &cfgs[i];

		if (le32_to_cpu(cfg->port) == 0)
			continue;

		if (print_level)
			printf(PRINT_LEVEL1, "");

		printf( "[%02zu]: P%c.%02d, "
			"mul = %d, pull = 0x%02x, driver = 0x%02x, data = 0x%02x\n",
			i, lichee_gpio_cfg_port_to_char(cfg->port),
			cfg->port_num,
			cfg->mul, cfg->pull, cfg->driver, cfg->data);

		print_level = 1;
	}
}



static int save_lichee_gpio_cfg_arrays(cJSON *parent, const char *name,
				       const void *addr, size_t sz)
{
	cJSON *json_gpios = cJSON_AddArrayToObject(parent, name);
	size_t gpio_sz = sizeof(struct lichee_gpio_cfg);
	const struct lichee_gpio_cfg *cfgs = addr;

	assert((sz % gpio_sz) == 0);
	if (!json_gpios)
		return -1;

	for (size_t i = 0; i < sz / gpio_sz; i++) {
		const struct lichee_gpio_cfg *cfg = &cfgs[i];
		cJSON *json;

		if (le32_to_cpu(cfg->port) == 0)
			continue;

		json = cJSON_CreateObject();
		if (!json)
			return -1;

		cJSON_AddItemToArray(json_gpios, json);

		json_format_add_string_value(json, "index", "%zu", i);
		json_format_add_string_value(json, "pin", "P%c.%02d",
					     lichee_gpio_cfg_port_to_char(cfg->port),
					     cfg->port_num);
		json_format_add_string_value(json, "mul", "%d", cfg->mul);

		/* pull, driver and data has a default value: 0xff */
		if (cfg->pull != 0xff)
			json_format_add_string_value(json, "pull", "%d", cfg->pull);
		if (cfg->driver != 0xff)
			json_format_add_string_value(json, "driver", "%d", cfg->driver);
		if (cfg->data != 0xff)
			json_format_add_string_value(json, "data", "%d", cfg->data);
	}

	return 0;
}
 /*
 * "jtag_gpios":	[{
 * 	"index":	"0",
 * 	"pin":	"PC.14",
 * 	"mul":	"3"
 * }, {
 * 	"index":	"1",
 * 	"pin":	"PC.15",
 * 	"mul":	"3"
 * }],
 */
static int load_lichee_gpio_cfg_arrays(cJSON *parent, const char *name,
				       void *addr, size_t sz)
{
	cJSON *item, *json_gpios = cJSON_GetObjectItem(parent, name);
	size_t max_items = sz / sizeof(struct lichee_gpio_cfg);
	struct lichee_gpio_cfg *cfgs = addr;

	assert((sz % sizeof(struct lichee_gpio_cfg)) == 0);
	if (!json_gpios) {
		fprintf(stderr, "Error: %s: <%s> is not found\n",
			__func__, name);
		return -1;
	} else if (!cJSON_IsArray(json_gpios)) {
		fprintf(stderr, "Error: %s: <%s> is not an array\n",
			__func__, name);
		return -1;
	}

	cJSON_ArrayForEach(item, json_gpios) {
		struct lichee_gpio_cfg *cfg;
		const char *pin;
		int index;
		long tmp;

		index = json_string_value_to_number_or_default(item, "index", 10, -1);
		if (index < 0 || index >= (int)max_items) {
			fprintf(stderr, "Error: %s: %s's index %d is overflow\n",
				__func__, name, index);
			return -1;
		}
		cfg = &cfgs[index];

		pin = json_get_string_value_in_object(item, "pin");
		if (!pin) {
			fprintf(stderr, "Error: %s: %s's pin is not defined\n",
				__func__, name);
			return -1;
		} else if (strlen(pin) != strlen("PA.00") || pin[0] != 'P' || pin[2] != '.') {
		bad_pin:
			fprintf(stderr, "Error: %s: %s's pin(%s) is invalid\n",
				__func__, name, pin);
			return -1;
		}

		cfg->port = char_to_lichee_gpio_cfg_port(pin[1]);
		cfg->port_num = strict_strtol_or_default(pin + 3, 10, NULL, -1);
		if (cfg->port_num >= 32)
			goto bad_pin;

		tmp = json_string_value_to_number_or_default(item, "mul", 10, -1);
		if (tmp < 0) {
			fprintf(stderr, "Error: %s: %s's mul is not defined\n",
				__func__, name);
			return -1;
		} else if (tmp > 7) {
			/* the max available mul number is 7 based on allwinner's datasheet */
			fprintf(stderr, "Error: %s: %s's mul is out of range\n",
				__func__, name);
			return -1;
		}
		cfg->mul = (uint8_t)tmp;

		/* pull, driver and data are optional, which has the default value: 0xff */
		cfg->pull = (uint8_t)json_string_value_to_number_or_default(item, "pull", 10, -1);
		cfg->driver = (uint8_t)json_string_value_to_number_or_default(item, "driver", 10, -1);
		cfg->data = (uint8_t)json_string_value_to_number_or_default(item, "data", 10, -1);
	}

	return 0;
}

static void print_lichee_dram_para(const char *print_name_fmt, const char *name,
				   const void *addr, size_t sz)
{
	structure_print_name(print_name_fmt, name);
	putchar('\n');

	structure_print(PRINT_LEVEL1, addr, structure_lichee_dram_para);
}

static int save_lichee_dram_para(cJSON *json, const char *name,
				 const void *addr, size_t sz)
{
	return structure_save_json(json, addr, structure_lichee_dram_para);
}

static int load_lichee_dram_para(cJSON *json, const char *name,
				 void *addr, size_t sz)
{
	return structure_load_json(json, addr, structure_lichee_dram_para);
}

static void print_lichee_storage_data(const char *print_name_fmt,
				      const char *name,
				      const void *addr, size_t sz)
{
	const uint32_t *buf32 = addr;

	structure_print_name(print_name_fmt, name);

	for (size_t i = 0; i < sz / sizeof(*buf32); i++) {
		if (i != 0 && (i % 8) == 0)
			printf("\n" PRINT_LEVEL1, "");
		printf("%08x ", le32_to_cpu(buf32[i]));
	}

	putchar('\n');
}

static const struct structure_item structure_lichee_private_head[] = {
	STRUCTURE_ITEM(struct lichee_private_head, debug_mode,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_private_head, dram_para,		print_lichee_dram_para,
		       .save_json = save_lichee_dram_para,
		       .load_json = load_lichee_dram_para),
	STRUCTURE_ITEM(struct lichee_private_head, uart_port,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_private_head, uart_gpios,		print_lichee_gpio_cfg_arrays,
		       .save_json = save_lichee_gpio_cfg_arrays,
		       .load_json = load_lichee_gpio_cfg_arrays),
	STRUCTURE_ITEM(struct lichee_private_head, enable_jtag,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct lichee_private_head, jtag_gpios,		print_lichee_gpio_cfg_arrays,
		       .save_json = save_lichee_gpio_cfg_arrays,
		       .load_json = load_lichee_gpio_cfg_arrays),
	STRUCTURE_ITEM(struct lichee_private_head, storage_gpios,	print_lichee_gpio_cfg_arrays,
		      .save_json = save_lichee_gpio_cfg_arrays,
		      .load_json = load_lichee_gpio_cfg_arrays),
	STRUCTURE_ITEM(struct lichee_private_head, storage_data,	print_lichee_storage_data,
		      .save_json = structure_item_save_json_x32_array,
		      .load_json = structure_item_load_json_x32_array),
	STRUCTURE_ITEM_END(),
};

#define eGON_WITH_PRIV_HEAD		(1 << 0)
#define eGON_WITH_COMMIT_HASH		(1 << 1)

struct eGON_editor_private_data {
	struct boot_header		boot_head;
	struct lichee_private_head	priv_head;

	/* the default commit_hash size defined in u-boot-2014.07 is 64 bytes */
	char				commit_hash[1024];

	unsigned long			flags;
};

static void eGON_update_checksum(uint8_t *buf, size_t buster_sz, void *arg)
{
	uint32_t *sum = arg, *buf32 = (uint32_t *)buf;

	for (size_t i = 0; i < buster_sz / sizeof(*buf32); i++)
		(*sum) += buf32[i];
}

static int eGON_detect(void *private_data, int force_type, int fd)
{
	struct eGON_editor_private_data *p = private_data;
	uint32_t boot_offset, sum = STAMP_VALUE;
	int ret;

	static_assert(sizeof(p->boot_head) == 48, "struct boot_header");

	ret = read(fd, &p->boot_head, sizeof(p->boot_head));
	if (ret < 0)
		return ret;

	if (memcmp(p->boot_head.magic, BOOT0_MAGIC, sizeof(p->boot_head.magic))) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	if (le32_to_cpu(p->boot_head.this_struct_size) != sizeof(p->boot_head)) {
		fprintf_if_force_type("Error: boot_head size doesn't match\n");
		return -1;
	}

	boot_offset = get_boot_bin_offset(&p->boot_head);
	if (boot_offset >= sizeof(p->boot_head) + sizeof(p->priv_head)) {
		ret = read(fd, &p->priv_head, sizeof(p->priv_head));
		if (ret < 0)
			return ret;
		p->flags |= eGON_WITH_PRIV_HEAD;
	}

	if (boot_offset > sizeof(p->boot_head) + sizeof(p->priv_head)) {
		size_t sz = boot_offset - sizeof(p->boot_head) - sizeof(p->priv_head);

		if (sz > sizeof(p->commit_hash))
			sz = sizeof(p->commit_hash);

		ret = read(fd, &p->commit_hash, sz - 1);
		if (ret < 0)
			return ret;

		p->flags |= eGON_WITH_COMMIT_HASH;
	}

	/* verify the checksum */
	dd(fd, -1, 0, 0, le32_to_cpu(p->boot_head.length), eGON_update_checksum, &sum);
	sum -= le32_to_cpu(p->boot_head.checksum);
	if (sum != le32_to_cpu(p->boot_head.checksum)) {
		fprintf_if_force_type("Error: checksum doesn't match\n");
		return -1;
	}

	return 0;
}

static int eGON_list(void *private_data, int fd, int argc, char **argv)
{
	struct eGON_editor_private_data *p = private_data;

	structure_print(PRINT_LEVEL0, &p->boot_head, structure_boot_header);

	if (p->flags & eGON_WITH_PRIV_HEAD) {
		printf("\nlichee boot0 private header:\n");
		structure_print(PRINT_LEVEL0, &p->priv_head,
				structure_lichee_private_head);
	}

	if (p->flags & eGON_WITH_COMMIT_HASH) {
		printf("\ncommit hash:\n");
		printf(PRINT_LEVEL0 "%s\n", "hash", p->commit_hash);
	}

	return 0;
}

static int eGON_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	struct eGON_editor_private_data *p = private_data;
	uint32_t boot_offset = get_boot_bin_offset(&p->boot_head);
	cJSON *root = cJSON_CreateObject();
	char filename[512];
	int outfd;
	int ret;

	if (!root)
		return -1;

	ret = structure_save_json(root, &p->boot_head, structure_boot_header);
	if (ret < 0)
		goto done;

	if (p->flags & eGON_WITH_PRIV_HEAD) {
		ret = structure_save_json(root, &p->priv_head, structure_lichee_private_head);
		if (ret < 0)
			goto done;
	}

	if (p->flags & eGON_WITH_COMMIT_HASH)
		json_add_string_value(root, "commit", p->commit_hash);

	snprintf(filename, sizeof(filename), "%s/boot_head.json", outdir);
	ret = json_saveto_file(root, filename);

	snprintf(filename, sizeof(filename), "%s/boot.bin", outdir);
	outfd = fileopen(filename, O_RDWR | O_CREAT, 0664);
	if (outfd < 0)
		goto done;

	boot_offset = get_boot_bin_offset(&p->boot_head);
	dd(fd, outfd, boot_offset, 0,
		le32_to_cpu(p->boot_head.length) - boot_offset, NULL, NULL);
done:
	cJSON_Delete(root);
	return ret;
}

static int eGON_pack(void *private_data, const char *dir, int fd_outimg, int argc, char **argv)
{
	struct eGON_editor_private_data *p = private_data;
	unsigned long boot_offset;
	long boot_size;
	int fd_boot = -1;
	char filename[512];
	uint32_t sum = 0;
	cJSON *json;
	int ret;

	memcpy(&p->boot_head.magic, BOOT0_MAGIC, sizeof(p->boot_head.magic));
	p->boot_head.this_struct_size = cpu_to_le32(sizeof(p->boot_head));

	snprintf(filename, sizeof(filename), "%s/boot_head.json", dir);
	json = json_from_file(filename);
	if (!json) {
		fprintf(stderr, "Error: can't load %s\n", filename);
		return -1;
	}

	ret = structure_load_json(json, &p->boot_head, structure_boot_header);
	if (ret < 0) {
		fprintf(stderr, "Error: load boot head failed\n");
		goto done;
	}

	boot_offset = get_boot_bin_offset(&p->boot_head);
	if (boot_offset >= sizeof(p->boot_head) + sizeof(p->priv_head))
		p->flags |= eGON_WITH_PRIV_HEAD;
	if (boot_offset > sizeof(p->boot_head) + sizeof(p->priv_head))
		p->flags |= eGON_WITH_COMMIT_HASH;

	if (p->flags & eGON_WITH_PRIV_HEAD) {
		ret = structure_load_json(json, &p->priv_head, structure_lichee_private_head);
		if (ret < 0) {
			fprintf(stderr, "Error: load pirvate head failed\n");
			goto done;
		}
	}

	if (p->flags & eGON_WITH_COMMIT_HASH) {
		const char *commit = json_get_string_value_in_object(json, "commit");

		if (!commit) {
			fprintf(stderr, "Error: commit hash not found\n");
			ret = -1;
			goto done;
		}

		snprintf(p->commit_hash, sizeof(p->commit_hash), "%s", commit);
	}

	snprintf(filename, sizeof(filename), "%s/boot.bin", dir);
	fd_boot = fileopen(filename, O_RDONLY, 0);
	if (fd_boot < 0) {
		fprintf(stderr, "Error: can't open %s\n", filename);
		ret = fd_boot;
		goto done;
	}

	boot_size = (long)filelength(fd_boot);
	if (boot_size < 0) {
		fprintf(stderr, "Error: can't detect file length %s\n",
			filename);
		ret = -1;
		goto done;
	}

	if (boot_size + boot_offset > le32_to_cpu(p->boot_head.length)) {
		fprintf(stderr, "Error: boot file is too large\n");
		ret = -1;
		goto done;
	}

	dd(fd_boot, fd_outimg, 0, boot_offset, boot_size, NULL, NULL);
	long remain = le32_to_cpu(p->boot_head.length) - boot_size - boot_offset;
	if (remain > 0) {
		/* pading with zero until boot_head.length */
		long remain = le32_to_cpu(p->boot_head.length) - boot_size - boot_offset;
		dd(-1, fd_outimg, 0, boot_offset + boot_size, remain, NULL, NULL);
	}

	/* calc checksum and write head */
	dd(fd_outimg, -1, boot_offset, 0,
	   le32_to_cpu(p->boot_head.length) - boot_offset,
	   eGON_update_checksum, &sum);

	uint32_t commit_hash_sz = boot_offset - sizeof(p->boot_head) - sizeof(p->priv_head);

	p->boot_head.checksum = cpu_to_le32(STAMP_VALUE);
	eGON_update_checksum((uint8_t *)&p->boot_head, sizeof(p->boot_head), &sum);

	if (p->flags & eGON_WITH_PRIV_HEAD)
		eGON_update_checksum((uint8_t *)&p->priv_head, sizeof(p->priv_head), &sum);
	if (p->flags & eGON_WITH_COMMIT_HASH)
		eGON_update_checksum((uint8_t *)p->commit_hash, commit_hash_sz, &sum);
	p->boot_head.checksum = cpu_to_le32(sum);

	lseek(fd_outimg, 0, SEEK_SET);
	write(fd_outimg, &p->boot_head, sizeof(p->boot_head));
	if (p->flags & eGON_WITH_PRIV_HEAD)
		write(fd_outimg, &p->priv_head, sizeof(p->priv_head));
	if (p->flags & eGON_WITH_COMMIT_HASH)
		write(fd_outimg, &p->commit_hash, commit_hash_sz);

done:
	if (!(fd_boot < 0))
		close(fd_boot);
	cJSON_Delete(json);
	return ret;
}

static struct imgeditor eGON_editor = {
	.name			= "eGON",
	.descriptor		= "allwinner eGON image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct boot_header)
					+ sizeof(struct lichee_private_head),
	.private_data_size	= sizeof(struct eGON_editor_private_data),
	.detect			= eGON_detect,
	.unpack			= eGON_unpack,
	.pack			= eGON_pack,
	.list			= eGON_list,
};
REGISTER_IMGEDITOR(eGON_editor);
