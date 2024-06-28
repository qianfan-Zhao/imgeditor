/*
 * Extended ini parser
 * qianfan Zhao
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "exini.h"
#include "string_helper.h"

static struct exini_section *exini_alloc_section(const char *name, size_t idx)
{
	struct exini_section *section = calloc(1, sizeof(*section));

	if (!section)
		return section;

	list_init(&section->head);
	list_init(&section->properties);
	if (name)
		snprintf(section->name, sizeof(section->name), "%s", name);
	section->global_index = idx;

	return section;
}

int exini_add_property(struct exini_section *section, const char *name,
		       int data_type, void *data, size_t data_sz)
{
	struct exini_property *prop = calloc(1, data_sz + sizeof(*prop));

	if (!prop)
		return -1;

	list_init(&prop->head);
	snprintf(prop->name, sizeof(prop->name), "%s", name);
	prop->data_type = data_type;
	prop->data_sz = data_sz;

	if (data_sz)
		memcpy(prop->data, data, data_sz);

	list_add_tail(&prop->head, &section->properties);
	section->n_properties++;

	return 0;
}

int exini_default_kv_handler(struct exini_section *section, int enable_unknown,
			     char *key, char *value)
{
	int type = -1;
	uint8_t data[128];
	size_t sz = 0;

	if (value[0] == '\0') {
		type = EXINI_PROP_DATA_TYPE_NULL;
	} else if (value[0] == '"' && value[strlen(value) - 1] == '"') {
		char *s = value + 1;

		s[strlen(s) - 1] = '\0';
		snprintf((char *)data, sizeof(data), "%s", s);

		type = EXINI_PROP_DATA_TYPE_STRING;
		sz = strlen((char *)data) + 1; /* including null */
	} else {
		unsigned long ul;
		char *endp;

		ul = strtoul(value, &endp, 0);
		if (*endp == 0) {
			type = EXINI_PROP_DATA_TYPE_ULONG;
			sz = sizeof(ul);
			memcpy(&data, &ul, sizeof(ul));
		} else if (enable_unknown) {
			type = EXINI_PROP_DATA_TYPE_UNKNOWN;

			snprintf((char *)data, sizeof(data), "%s", value);
			sz = strlen((char *)data) + 1;
		}
	}

	if (type < 0) {
		fprintf(stderr, "Error: unsupported property: %s = %s\n",
			key, value);
		return -1;
	}

	if (exini_add_property(section, key, type, data, sz) < 0) {
		fprintf(stderr, "Error: alloc property failed\n");
		return -1;
	}

	return 0;
}

static int exini_load_property(struct exini *ini, struct exini_section *section,
			       int linenum, char *s)
{
	char *key, *value, *equal;
	int ret;

	/* new property */
	if (string_count_char(s, '=') != 1) {
		fprintf(stderr, "Error: #%d bad property\n", linenum);
		fprintf(stderr, "%s\n", s);
		return -1;
	} else if (!section) {
		fprintf(stderr, "Error: #%d dissociative property\n",
			linenum);
		return -1;
	}

	equal = strchr(s, '=');
	*equal = '\0';

	key = string_skip_head_space(s);
	value = string_skip_head_space(equal + 1);
	string_remove_eol_space(key);
	string_remove_eol_space(value);

	/* both the key and value can not container space */
	if (string_count_space(key) > 0) {
		fprintf(stderr, "Error: #%d key property has space\n",
			linenum);
		return -1;
	}

	if (value[0] != '"' && string_count_space(value) > 0) {
		fprintf(stderr, "Error: #%d property value has space\n",
			linenum);
		return -1;
	}

	if (ini->prop_handler)
		ret = ini->prop_handler(section, key, value, ini->private_data);
	else
		ret = exini_default_kv_handler(section,
					ini->enable_unknown_data,
					key, value);
	return ret;
}

/* remove end of line space and any comment */
static char *remove_eol_comment(char *start)
{
	char *s = string_remove_eol_space(start);
	char *semicolon;

	semicolon = strrchr(s, ';');
	if (semicolon) {
		/* under string? */
		if (!strchr(s + 1, '\"'))
			*semicolon = '\0';
	}

	return start;
}

static int exini_do_load(struct exini *ini, FILE *fp)
{
	struct exini_section *section = NULL;
	char linebuf[1024];
	int linenum = 1;

	while (fgets(linebuf, sizeof(linebuf) - 1, fp)) {
		char *s = string_skip_head_space(linebuf);
		int len, ret;

		remove_eol_comment(s);
		len = strlen(s);

		/* the line started with ';' is comments */
		if (s[0] == '\0' || s[0] == ';')
			goto next_line;

		/* new section */
		if (s[0] == '[' && s[len - 1] == ']') {
			char *section_name = s + 1;

			s[len - 1] = '\0'; /* remove ']' */

			section = exini_alloc_section(section_name,
					ini->total_sections++);
			if (!section) {
				fprintf(stderr, "Error: alloc section failed\n");
				return -1;
			}

			list_add_tail(&section->head, &ini->sections);
			goto next_line;
		} else if (s[0] == '{') {
			char *prop_argv[64] = { NULL };
			int prop_argc;

			/* anonymous section */
			if (string_count_char(s, '{') != 1
			    || string_count_char(s, '}') != 1) {
				fprintf(stderr, "Error: #%d bad section\n",
					linenum);
				return -1;
			}

			section = exini_alloc_section(NULL,
						      ini->total_sections++);
			if (!section) {
				fprintf(stderr, "Error: alloc anonymous section failed\n");
				return -1;
			}

			list_add_tail(&section->head, &ini->sections);

			/* {filename = "board.fex", maintype = ITEM_COMMON, subtype = "BOARD_CONFIG_BIN", }, */
			prop_argc = argv_buffer_split_with_delim(s, "{},",
								 prop_argv,
								 64);
			for (int i = 0; i < prop_argc; i++) {
				char *prop = prop_argv[i];

				prop = string_skip_head_space(prop);
				string_remove_eol_space(prop);
				if (*prop == '\0')
					break;

				ret = exini_load_property(ini, section,
							  linenum,
							  prop);
				if (ret < 0)
					return ret;
			}

			goto next_line;
		}

		ret = exini_load_property(ini, section, linenum, s);
		if (ret < 0)
			return ret;

	next_line:
		linenum++;
	}

	return 0;
}

int exini_load(struct exini *ini, FILE *fp)
{
	int ret;

	list_init(&ini->sections);
	ini->total_sections = 0;

	ret = exini_do_load(ini, fp);
	if (ret < 0)
		exini_free(ini);

	return ret;
}

struct exini_section *exini_find_section(struct exini *ini, const char *name)
{
	struct exini_section *sec;

	list_for_each_entry(sec, &ini->sections, head, struct exini_section) {
		if (!strcmp(sec->name, name))
			return sec;
	}

	return NULL;
}

struct exini_section *exini_find_section_byid(struct exini *ini, size_t id)
{
	struct exini_section *sec;

	list_for_each_entry(sec, &ini->sections, head, struct exini_section) {
		if (sec->global_index == id)
			return sec;
	}

	return NULL;
}

struct exini_property *exini_find_section_property(struct exini_section *sec,
						   const char *name)
{
	struct exini_property *prop;

	list_for_each_entry(prop, &sec->properties, head,
			    struct exini_property) {
		if (!strcmp(prop->name, name))
			return prop;
	}

	return NULL;
}

struct exini_property *exini_find_property(struct exini *ini,
					   const char *section_name,
					   const char *prop_name)
{
	struct exini_section *sec = exini_find_section(ini, section_name);

	if (!sec)
		return NULL;

	return exini_find_section_property(sec, prop_name);
}

void exini_free(struct exini *ini)
{
	struct exini_section *section, *next_section;

	list_for_each_entry_safe(section, next_section, &ini->sections,
				 head, struct exini_section) {
		struct exini_property *prop, *next_prop;

		list_for_each_entry_safe(prop, next_prop, &section->properties,
					head, struct exini_property) {
			list_del(&prop->head);
			free(prop);
		}

		list_del(&section->head);
		free(section);
	}
}
