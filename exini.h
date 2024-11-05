/*
 * Extended ini parser
 * qianfan Zhao
 */

#ifndef EXINI_H
#define EXINI_H

#include <stdio.h>
#include "list_head.h"

struct exini_section {
	struct list_head		head;
	size_t				global_index;
	struct list_head		properties;
	size_t				n_properties;
	char				name[128];
};

enum {
	EXINI_PROP_DATA_TYPE_NULL,
	EXINI_PROP_DATA_TYPE_ULONG,		/* unsigned long number */
	EXINI_PROP_DATA_TYPE_STRING,		/* "hello" */
	EXINI_PROP_DATA_TYPE_UNKNOWN,		/* hello */

	EXINI_PROP_DATA_TYPE_CUSTOM = 0x80,
};

struct exini_property {
	struct list_head		head;
	char				name[128];
	int				data_type;
	size_t				data_sz;
	uint8_t				data[0];
};

typedef int (*exini_prop_handler_cb_t)(struct exini_section *section,
				       char *key, char *value,
				       void *data);

#define EXINI_QUIRK_ANONYMOUS_SECTION	(1 << 0)
#define EXINI_QUIRK_DTBOCFG		(1 << 1)

struct exini {
	struct list_head		sections;
	int				enable_unknown_data;
	exini_prop_handler_cb_t		prop_handler;
	size_t				total_sections;
	void				*private_data;

	unsigned int			quirks;
};

int exini_load(struct exini *ini, FILE *fp, unsigned int quirks);
void exini_free(struct exini *ini);

struct exini_section *exini_find_section_byid(struct exini *ini, size_t id);
struct exini_section *exini_find_section(struct exini *ini, const char *name);
struct exini_property *exini_find_section_property(struct exini_section *sec,
						   const char *name);
struct exini_property *exini_find_property(struct exini *ini,
					   const char *section_name,
					   const char *prop_name);

enum {
	EXINI_ERR_NOT_DEFINED = 1,
	EXINI_ERR_BAD_TYPE,
};

unsigned long exini_property_read_ulong(struct exini_property *prop,
					unsigned long def,
					int *err);
unsigned long exini_section_read_ulong(struct exini_section *sec,
					const char *prop_name,
					unsigned long def,
					int *err);
unsigned long exini_read_ulong(struct exini *ini,
			       const char *section_name,
			       const char *prop_name,
			       unsigned long def,
			       int *err);

const char *exini_property_read_string(struct exini_property *prop,
					const char *def,
					int enable_unknown,
					int *err);
const char *exini_section_read_string(struct exini_section *sec,
				      const char *prop_name,
				      const char *def,
				      int enable_unknown,
				      int *err);
const char *exini_read_string(struct exini *ini, const char *section_name,
			      const char *prop_name, const char *def,
			      int *err);

int exini_default_kv_handler(struct exini_section *section,
			     int enable_unknown,
			     char *key, char *value);

int exini_add_property(struct exini_section *section, const char *name,
		       int data_type, void *data, size_t data_sz);

#endif
