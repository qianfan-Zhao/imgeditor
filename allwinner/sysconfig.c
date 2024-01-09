/*
 * allwinner sys-config.fex image editor
 * qianfan Zhao <qianfanguijin@163.com>
 * 2024-01-09
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "imgeditor.h"
#include "structure.h"

/* The binary sys-config file format:
 *
 * syscfg_bin_head
 *
 * [syscfg_bin_section]
 *     syscfg_bin_property1
 *     syscfg_bin_property2
 *     ...
 *     syscfg_bin_propertyN
 */
struct syscfg_bin_head {
	__le32				sections;
	__le32				filesize;
	__le32				version_major;
	__le32				version_minor;
};

#define SYSCFG_BIN_SECTION_NAME_SZ	32
#define SYSCFG_BIN_PROPERTY_NAME_SZ	32

struct syscfg_bin_section {
	char				name[SYSCFG_BIN_SECTION_NAME_SZ];
	__le32				property_count;
	/* offset * 4 is the byte offset location */
	__le32				offset;
};

struct syscfg_bin_property {
	char				name[SYSCFG_BIN_PROPERTY_NAME_SZ];
	/* offset * 4 is the byte offset location */
	__le32				offset;
	__le32				pattern;
};

struct syscfg_bin_property_gpio {
	__le32				port;
	__le32				num;
	__le32				mul;
	__le32				pull;
	__le32				driver;
	__le32				data;
};

enum syscfg_property_type {
	PROPERTY_TYPE_SINGLE_WORD = 1,
	PROPERTY_TYPE_STRING,
	PROPERTY_TYPE_MULTI_WORD,
	PROPERTY_TYPE_GPIO,
	PROPERTY_TYPE_NULL,
};

struct syscfg_section {
	struct list_head		head;
	struct list_head		properties;
	char				name[SYSCFG_BIN_SECTION_NAME_SZ];
};

struct syscfg_property {
	struct list_head		head;
	enum syscfg_property_type	type;
	char				name[SYSCFG_BIN_PROPERTY_NAME_SZ];
	uint8_t				data[0];
};

struct syscfg_private {
	struct syscfg_bin_head		head;

	struct list_head		sections;
};

static struct syscfg_section *syscfg_alloc_section(struct syscfg_private *p,
						   const char *name)
{
	struct syscfg_section *section = calloc(sizeof(*section), 1);

	if (section) {
		list_init(&section->head);
		list_init(&section->properties);
		snprintf(section->name, sizeof(section->name), "%s", name);

		list_add_tail(&section->head, &p->sections);
	}

	return section;
}

static struct syscfg_property *
	syscfg_alloc_property(struct syscfg_section *section, const char *name,
			      enum syscfg_property_type type,
			      uint32_t data_sz, const void *data)
{
	struct syscfg_property *prop = calloc(sizeof(*prop) + data_sz, 1);

	if (prop) {
		list_init(&prop->head);
		list_add_tail(&prop->head, &section->properties);

		prop->type = type;
		snprintf(prop->name, sizeof(prop->name), "%s", name);
		memcpy(prop->data, data, data_sz);
	}

	return prop;
}

static int syscfg_private_init(void *private_data)
{
	struct syscfg_private *p = private_data;

	list_init(&p->sections);
	return 0;
}

static void syscfg_private_exit(void *private_data)
{
	struct syscfg_private *p = private_data;
	struct syscfg_section *section, *next_section;

	list_for_each_entry_safe(section, next_section, &p->sections, head,
				 struct syscfg_section)	{
		struct syscfg_property *prop, *next_prop;

		list_for_each_entry_safe(prop, next_prop, &section->properties,
					 head, struct syscfg_property) {
			list_del(&prop->head);
			free(prop);
		}

		list_del(&section->head);
		free(section);
	}
}

static void syscfg_bin_property_read_pattern(struct syscfg_bin_property *prop,
					     uint16_t *type, uint16_t *words)
{
	uint32_t pattern = le32_to_cpu(prop->pattern);

	*type =  (pattern >> 16) & 0xffff;
	*words = (pattern >>  0) & 0xffff;
}

static int syscfg_bin_section_check(struct syscfg_bin_section *bs, int idx,
				    int force_type, void *mem, uint32_t memlen)
{
	struct syscfg_bin_property *bp;
	uint32_t offset, n_prop;

	offset = le32_to_cpu(bs->offset) << 2;
	n_prop = le32_to_cpu(bs->property_count);

	if (get_verbose_level() > 1) {
		printf("%-20s 0x%08x %2d\n",
			bs->name, offset, n_prop);
	}

	if (bs->name[0] == '\0') {
		fprintf_if_force_type("Error: section %d no name\n", idx);
		return -1;
	} else if (offset >= memlen) {
		fprintf_if_force_type("Error: section %d [%s] is "
					"overflow\n",
					idx, bs->name);
		return -1;
	} else if (offset + n_prop * sizeof(*bp) >= memlen) {
		fprintf_if_force_type("Error: section %d [%s]'s "
					"properties is overflow\n",
					idx, bs->name);
		return -1;
	}

	bp = mem + offset;
	for (uint32_t i = 0; i < n_prop; i++) {
		uint16_t type, words, expected_words;

		offset = le32_to_cpu(bp[i].offset) << 2;
		syscfg_bin_property_read_pattern(&bp[i], &type, &words);
		expected_words = words;

		if (get_verbose_level() > 1) {
			printf("    %-16s 0x%08x 0x%04x %d\n",
				bp[i].name, offset, type, words);
		}

		if ((offset + words * 4) > memlen) {
			fprintf_if_force_type("Error: The %d property(%s) of %s "
					      "is overflow\n",
					      i, bp[i].name, bs->name);
			return -1;
		}

		switch (type) {
		case PROPERTY_TYPE_GPIO:
			expected_words =
				sizeof(struct syscfg_bin_property_gpio) >> 2;
			break;
		case PROPERTY_TYPE_SINGLE_WORD:
			expected_words = 1;
			break;
		case PROPERTY_TYPE_STRING:
		case PROPERTY_TYPE_NULL:
			break;
		default:
			fprintf_if_force_type("Error: The %d property(%s) of %s "
					      "has unsupported parrten 0x%08x\n",
					      i, bp[i].name, bs->name,
					      le32_to_cpu(bp[i].pattern));
			return -1;
		}

		if (expected_words != words) {
			fprintf_if_force_type("Error: The words of %d property"
					      "(%s) in %s is doesn't match\n",
					      i, bp[i].name, bs->name);
			return -1;
		}
	}

	return 0;
}

static int syscfg_from_bin(struct syscfg_private *p, int force_type, void *mem,
			   uint32_t memlen)
{
	struct syscfg_bin_section *bin_sections = mem + sizeof(p->head);

	for (uint32_t i = 0; i < le32_to_cpu(p->head.sections); i++) {
		struct syscfg_bin_section *bin_section = bin_sections + i;
		struct syscfg_bin_property *bin_properties;
		struct syscfg_section *section;
		int ret;

		bin_properties = mem + (le32_to_cpu(bin_section->offset) << 2);

		ret = syscfg_bin_section_check(bin_section, i, force_type,
					       mem, memlen);
		if (ret < 0)
			return ret;

		section = syscfg_alloc_section(p, bin_section->name);
		if (!section)
			continue;

		for (uint32_t i = 0;
		     i < le32_to_cpu(bin_section->property_count); i++) {
			struct syscfg_bin_property *bp = bin_properties + i;
			uint32_t offset = le32_to_cpu(bp->offset) << 2;
			uint16_t type, words;

			syscfg_bin_property_read_pattern(bp, &type, &words);

			syscfg_alloc_property(section, bp->name, type,
					      words << 2, mem + offset);
		}
	}

	return 0;
}

static int syscfg_detect(void *private_data, int force_type, int fd)
{
	uint32_t total_length = (uint32_t)filelength(fd);
	struct syscfg_private *p = private_data;
	uint8_t *mem;
	int ret;

	ret = read(fd, &p->head, sizeof(p->head));
	if (ret < 0)
		return ret;

	if (le32_to_cpu(p->head.version_major) > 0x10 ||
		le32_to_cpu(p->head.version_minor) > 0x10) {
		fprintf_if_force_type("Error: bad version: 0x%08x-%08x\n",
				      le32_to_cpu(p->head.version_major),
				      le32_to_cpu(p->head.version_minor));
		return -1;
	}

	if (le32_to_cpu(p->head.sections) > 0x100) {
		fprintf_if_force_type("Error: too much sections: %d\n",
				      le32_to_cpu(p->head.sections));
		return -1;
	}

	if (le32_to_cpu(p->head.filesize) != total_length ||
		total_length <= sizeof(p->head)) {
		fprintf_if_force_type("Error: filesize doesn't match\n");
		return -1;
	}

	mem = malloc(total_length);
	if (!mem) {
		fprintf(stderr, "Error: alloc %d bytes failed\n", total_length);
		return -1;
	}

	lseek(fd, 0, SEEK_SET);
	read(fd, mem, total_length);
	ret = syscfg_from_bin(p, force_type, mem, total_length);
	free(mem);

	return ret;
}

static void syscfg_gpio_property_print(FILE *fp, __le32 *buf, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		int n = le32_to_cpu(buf[i]);

		if (n < 0)
			fprintf(fp, "<default>");
		else
			fprintf(fp, "<%d>", n);
	}
}

static int property_is_in_hexmode(const char *prop_name)
{
	static const char *hexa_entries[] = {
		"dram_baseaddr", "dram_zq", "dram_tpr", "dram_emr",
		"g2d_size",
		"rtp_press_threshold", "rtp_sensitive_level",
		"ctp_twi_addr", "csi_twi_addr", "csi_twi_addr_b", "tkey_twi_addr",
		"lcd_gamma_tbl_",
		"gsensor_twi_addr",
		NULL
	};

	for (int i = 0; hexa_entries[i]; i++) {
		const char *s = hexa_entries[i];
		int len = strlen(s);

		if (!strcmp(prop_name, s))
			return 1; /* true */

		/* dram_tpr can match dram_tpr%d */
		if (!strncmp(prop_name, s, len))
			return isdigit(prop_name[len]);
	}

	return 0; /* false */
}

static int syscfg_dump(struct syscfg_private *p, FILE *fp)
{
	struct syscfg_section *section;

	list_for_each_entry(section, &p->sections, head, struct syscfg_section) {
		struct syscfg_property *prop;

		fprintf(fp, "[%s]\n", section->name);

		list_for_each_entry(prop, &section->properties, head,
				    struct syscfg_property) {
			struct syscfg_bin_property_gpio *gpio = (void *)prop->data;
			__le32 *p_le32 = (__le32 *)prop->data;

			fprintf(fp, "%s =", prop->name);
			if (prop->type != PROPERTY_TYPE_NULL)
				fprintf(fp, " ");

			switch (prop->type) {
			case PROPERTY_TYPE_NULL:
				break;
			case PROPERTY_TYPE_SINGLE_WORD:
				if (property_is_in_hexmode(prop->name))
					fprintf(fp, "0x%x", le32_to_cpu(*p_le32));
				else
					fprintf(fp, "%d", le32_to_cpu(*p_le32));
				break;
			case PROPERTY_TYPE_STRING:
				fprintf(fp, "\"%s\"", prop->data);
				break;
			case PROPERTY_TYPE_GPIO:
				fprintf(fp, "port:");

				if (le32_to_cpu(gpio->port) == 0xffff)
					fprintf(fp, "power%d",
						le32_to_cpu(gpio->num));
				else
					fprintf(fp, "P%c%02d", 'A' +
						le32_to_cpu(gpio->port) - 1,
						le32_to_cpu(gpio->num));

				/* print mul, pull, driver, data */
				syscfg_gpio_property_print(fp, &gpio->mul, 4);
				break;
			default:
				fprintf(fp, "??");
				break;
			}

			fprintf(fp, "\n");
		}

		fprintf(fp, "\n");
	}

	return 0;
}

static int syscfg_list(void *private_data, int fd, int argc, char **argv)
{
	struct syscfg_private *p = private_data;

	return syscfg_dump(p, stdout);
}

static int syscfg_unpack(void *private_data, int fd, const char *out_name,
			    int argc, char **argv)
{
	struct syscfg_private *p = private_data;
	FILE *fp = fopen(out_name, "w+");
	int ret;

	if (!fp) {
		fprintf(stderr, "Error: open %s failed(%m)\n", out_name);
		return -1;
	}

	ret = syscfg_dump(p, fp);
	fclose(fp);

	return ret;
}

static struct imgeditor syscfg_editor = {
	.name			= "sysconfig",
	.descriptor		= "allwinner sysconfig image editor",
	.flags			= IMGEDITOR_FLAG_SINGLE_BIN,
	.header_size		= 128,
	.private_data_size	= sizeof(struct syscfg_private),
	.init			= syscfg_private_init,
	.exit			= syscfg_private_exit,
	.detect			= syscfg_detect,
	.list			= syscfg_list,
	.unpack			= syscfg_unpack,
};
REGISTER_IMGEDITOR(syscfg_editor);
