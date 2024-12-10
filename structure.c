#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "structure.h"
#include "imgeditor.h"
#include "json_helper.h"
#include "string_helper.h"

static enum structure_endian forced_endian = STRUCTURE_ENDIAN_FORCE_NONE;

enum structure_endian structure_force_endian(enum structure_endian set)
{
	enum structure_endian prev = forced_endian;

	forced_endian = set;
	return prev;
}

static int structure_endian_prefer_le(int le)
{
	switch (forced_endian) {
	case STRUCTURE_ENDIAN_FORCE_BE:
		return 0;
	case STRUCTURE_ENDIAN_FORCE_LE:
		return 1;
	default:
		break;
	}

	return le;
}

void structure_print_name(const char *print_name_fmt, const char *name)
{
	char print_fmt[64];

	snprintf(print_fmt, sizeof(print_fmt), "%s", print_name_fmt);
	printf(print_fmt, name);
}

typedef union {
	uint8_t		u8;
	uint16_t	u16;
	uint32_t	u32;
	uint64_t	u64;
} unsigned_t;

static uint64_t get_unsigned(int le, const unsigned_t *un, size_t sz)
{
	uint64_t n = 0;

	switch (sz) {
	case sizeof(uint8_t):
		n = un->u8;
		break;
	case sizeof(uint16_t):
		n = le ? le16_to_cpu(un->u16) : be16_to_cpu(un->u16);
		break;
	case sizeof(uint32_t):
		n = le ? le32_to_cpu(un->u32) : be32_to_cpu(un->u32);
		break;
	case sizeof(uint64_t):
		n = le ? le64_to_cpu(un->u64) : be64_to_cpu(un->u64);
		break;
	}

	return n;
}

static void snprintf_unsigned(char *s, size_t bufsz, int le, const unsigned_t *un,
			      size_t sz)
{
	switch (sz) {
	case sizeof(uint8_t):
		snprintf(s, bufsz, "%d", un->u8);
		break;
	case sizeof(uint16_t):
		snprintf(s, bufsz, "%d", le ? le16_to_cpu(un->u16) : be16_to_cpu(un->u16));
		break;
	case sizeof(uint32_t):
		snprintf(s, bufsz, "%d", le ? le32_to_cpu(un->u32) : be32_to_cpu(un->u32));
		break;
	case sizeof(uint64_t):
		snprintf(s, bufsz, "%" PRIu64, le ? le64_to_cpu(un->u64) : be64_to_cpu(un->u64));
		break;
	default:
		s[0] = '\0';
		break;
	}
}

static void snprintf_xunsigned(char *s, size_t bufsz, int le, const unsigned_t *un,
			       size_t sz)
{
	switch (sz) {
	case sizeof(uint8_t):
		snprintf(s, bufsz, "0x%02x", un->u8);
		break;
	case sizeof(uint16_t):
		snprintf(s, bufsz, "0x%04x", le ? le16_to_cpu(un->u16) : be16_to_cpu(un->u16));
		break;
	case sizeof(uint32_t):
		snprintf(s, bufsz, "0x%08x", le ? le32_to_cpu(un->u32) : be32_to_cpu(un->u32));
		break;
	case sizeof(uint64_t):
		snprintf(s, bufsz, "0x%016" PRIx64, le ? le64_to_cpu(un->u64) : be64_to_cpu(un->u64));
		break;
	default:
		s[0] = '\0';
		break;
	}
}

static void _structure_item_print_unsigned(const char *print_name_fmt,
					   const char *name,
					   int le,
					   const void *addr, size_t sz)
{
	char s[64];

	le = structure_endian_prefer_le(le);

	structure_print_name(print_name_fmt, name);
	snprintf_unsigned(s, sizeof(s), le, addr, sz);
	printf("%s\n", s);
}

void structure_item_print_unsigned(const char *print_name_fmt,
				   const char *name,
				   const void *addr, size_t sz)
{
	_structure_item_print_unsigned(print_name_fmt, name, 1, addr, sz);
}

void structure_item_print_be_unsigned(const char *print_name_fmt,
				      const char *name,
				      const void *addr, size_t sz)
{
	_structure_item_print_unsigned(print_name_fmt, name, 0, addr, sz);
}

static int _structure_item_save_json_unsigned(cJSON *json, const char *name,
					      int le,
					      const void *addr, size_t sz)
{
	char s[64];

	le = structure_endian_prefer_le(le);

	snprintf_unsigned(s, sizeof(s), le, addr, sz);

	return json_add_string_value(json, name, s);
}

int structure_item_save_json_unsigned(cJSON *json, const char *name,
				      const void *addr, size_t sz)
{
	return _structure_item_save_json_unsigned(json, name, 1, addr, sz);
}

int structure_item_save_json_be_unsigned(cJSON *json, const char *name,
					 const void *addr, size_t sz)
{
	return _structure_item_save_json_unsigned(json, name, 0, addr, sz);
}

static void _structure_item_print_xunsigned(const char *print_name_fmt,
					    const char *name,
					    int le,
					    const void *addr, size_t sz)
{
	char s[64];

	le = structure_endian_prefer_le(le);

	structure_print_name(print_name_fmt, name);
	snprintf_xunsigned(s, sizeof(s), le, addr, sz);
	printf("%s\n", s);
}

void structure_item_print_xunsigned(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_xunsigned(print_name_fmt, name, 1, addr, sz);
}

void structure_item_print_be_xunsigned(const char *print_name_fmt,
				       const char *name,
				       const void *addr, size_t sz)
{
	_structure_item_print_xunsigned(print_name_fmt, name, 0, addr, sz);
}

static void print_bit_descriptors(uint64_t flags,
				  const struct structure_bit_descriptor *d)
{
	unsigned index = 0;

	for (; d->descriptor; d++) {
		if (flags & d->value) {
			if (index != 0)
				printf(" ");
			printf("%s", d->descriptor);
			++index;
		}
	}
}

static void _structure_item_print_bit_flags(const char *print_name_fmt,
					    const char *name,
					    const void *addr, size_t sz,
					    int le,
					    const struct structure_bit_descriptor *d)
{
	uint64_t flags = get_unsigned(le, addr, sz);
	char s[64];

	le = structure_endian_prefer_le(le);

	structure_print_name(print_name_fmt, name);
	snprintf_xunsigned(s, sizeof(s), le, addr, sz);
	printf("%s (", s);
	print_bit_descriptors(flags, d);
	printf(")\n");
}

void structure_item_print_bit_flags(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz,
				    const struct structure_bit_descriptor *d)
{
	_structure_item_print_bit_flags(print_name_fmt, name, addr, sz, 1, d);
}

void structure_item_print_be_bit_flags(const char *print_name_fmt,
				       const char *name,
				       const void *addr, size_t sz,
				       const struct structure_bit_descriptor *d)
{
	_structure_item_print_bit_flags(print_name_fmt, name, addr, sz, 0, d);
}

void _structure_item_print_enum(const char *print_name_fmt, const char *name,
				const void *addr, size_t sz, int le,
				unsigned rshift, unsigned long mask,
				const struct structure_bit_descriptor *d)
{
	uint64_t n = get_unsigned(le, addr, sz);
	char s[64];

	n >>= rshift;
	if (mask)
		n &= mask;

	le = structure_endian_prefer_le(le);

	structure_print_name(print_name_fmt, name);
	snprintf_unsigned(s, sizeof(s), le, addr, sz);
	printf("%s (", s);

	for (; d->descriptor; d++) {
		if (n == d->value) {
			printf("%s", d->descriptor);
			break;
		}
	}
	if (!d->descriptor) /* not found */
		printf("%s", "???");

	printf(")\n");
}

void structure_item_print_enum(const char *print_name_fmt, const char *name,
			       const void *addr, size_t sz,
			       unsigned rshift, unsigned long mask,
			       const struct structure_bit_descriptor *d)
{
	_structure_item_print_enum(print_name_fmt, name, addr, sz, 1,
				   rshift, mask, d);
}

void structure_item_print_be_enum(const char *print_name_fmt, const char *name,
				  const void *addr, size_t sz,
				  unsigned rshift, unsigned long mask,
				  const struct structure_bit_descriptor *d)
{
	_structure_item_print_enum(print_name_fmt, name, addr, sz, 0,
				   rshift, mask, d);
}

static int _structure_item_save_json_xunsigned(cJSON *json, const char *name,
					       int le,
					       const void *addr, size_t sz)
{
	char s[64];

	le = structure_endian_prefer_le(le);

	snprintf_xunsigned(s, sizeof(s), le, addr, sz);

	return json_add_string_value(json, name, s);
}

int structure_item_save_json_xunsigned(cJSON *json, const char *name,
				       const void *addr, size_t sz)
{
	return _structure_item_save_json_xunsigned(json, name, 1, addr, sz);
}

int structure_item_save_json_be_xunsigned(cJSON *json, const char *name,
					  const void *addr, size_t sz)
{
	return _structure_item_save_json_xunsigned(json, name, 0, addr, sz);
}

static int _structure_item_load_json_unsigned(cJSON *json, const char *name,
					      int base, uint64_t *un)
{
	const char *s = json_get_string_value_in_object(json, name);
	int err = 0;

	if (!s)
		return -1;

	*un = (uint64_t)strict_strtoull(s, base, &err, NULL);
	return -err;
}

static int _structure_item_set_unsigned(void *addr, size_t sz, int le, uint64_t un)
{
	switch (sz) {
	case sizeof(uint8_t):
		*((uint8_t *)addr) = (uint8_t)un;
		break;
	case sizeof(uint16_t):
		*((uint16_t *)addr) = le ? cpu_to_le16(un) : cpu_to_be16(un);
		break;
	case sizeof(uint32_t):
		*((uint32_t *)addr) = le ? cpu_to_le32(un) : cpu_to_be32(un);
		break;
	case sizeof(uint64_t):
		*((uint64_t *)addr) = le ? cpu_to_le64(un) : cpu_to_be64(un);
		break;
	default:
		return -1;
	}

	return 0;
}

static int _structure_item_load_set_json_value(cJSON *json, const char *name,
					       int le, int base,
					       void *addr, size_t sz)
{
	uint64_t un;
	int ret;

	le = structure_endian_prefer_le(le);

	ret = _structure_item_load_json_unsigned(json, name, base, &un);
	if (ret < 0)
		return ret;

	return _structure_item_set_unsigned(addr, sz, le, un);
}

int structure_item_load_json_unsigned(cJSON *json, const char *name,
				      void *addr, size_t sz)
{
	return _structure_item_load_set_json_value(json, name, 1, 10, addr, sz);
}

int structure_item_load_json_xunsigned(cJSON *json, const char *name,
				       void *addr, size_t sz)
{
	return _structure_item_load_set_json_value(json, name, 1, 16, addr, sz);
}

int structure_item_load_json_be_unsigned(cJSON *json, const char *name,
					 void *addr, size_t sz)
{
	return _structure_item_load_set_json_value(json, name, 0, 10, addr, sz);
}

int structure_item_load_json_be_xunsigned(cJSON *json, const char *name,
					  void *addr, size_t sz)
{
	return _structure_item_load_set_json_value(json, name, 0, 16, addr, sz);
}

void structure_item_print_x8_array(const char *print_name_fmt, const char *name,
				   const void *addr, size_t sz)
{
	const uint8_t *p = addr;

	structure_print_name(print_name_fmt, name);
	for (size_t i = 0; i < sz; i++)
		printf("%02x ", p[i]);
	printf("\n");
}

int structure_item_save_json_x8_array(cJSON *json, const char *name,
				      const void *addr, size_t sz)
{
	cJSON *array = cJSON_CreateArray();
	const uint8_t *u8 = addr;

	if (!array)
		return -1;

	cJSON_AddItemToObject(json, name, array);

	for (size_t i = 0; i < sz; i++) {
		cJSON *item;
		char s[8];

		snprintf(s, sizeof(s), "0x%02x", u8[i]);
		item = cJSON_CreateString(s);
		if (!item)
			return -1;

		cJSON_AddItemToArray(array, item);

	}

	return 0;
}

int structure_item_load_json_x8_array(cJSON *json, const char *name,
				      void *addr, size_t sz)
{
	cJSON *item, *array = cJSON_GetObjectItem(json, name);
	uint8_t *u8 = addr;
	size_t i = 0;

	if (!array || !cJSON_IsArray(array))
		return -1;

	cJSON_ArrayForEach(item, array) {
		const char *s = item->valuestring;
		int err = 0;

		if (!(i < sz))
			break;

		u8[i++] = (uint8_t)strict_strtol(s, 16, &err, NULL);
		if (err)
			return -1;
	}

	return 0;
}

static int _structure_item_save_json_n32_array(cJSON *json, const char *name,
					       int le, const char *fmt,
					       const void *addr, size_t sz)
{
	cJSON *array = cJSON_CreateArray();
	const uint32_t *n32 = addr;

	le = structure_endian_prefer_le(le);

	if (!array)
		return -1;

	cJSON_AddItemToObject(json, name, array);

	for (size_t i = 0; i < sz / sizeof(*n32); i++) {
		cJSON *item;
		char s[32];

		snprintf(s, sizeof(s), fmt,
			 le ? le32_to_cpu(n32[i]) : be32_to_cpu(n32[i]));
		item = cJSON_CreateString(s);
		if (!item)
			return -1;

		cJSON_AddItemToArray(array, item);

	}

	return 0;
}

static int _structure_item_load_json_n32_array(cJSON *json, const char *name,
					       int le, int base,
					       void *addr, size_t sz)
{
	cJSON *item, *array = cJSON_GetObjectItem(json, name);
	size_t i = 0, max_items = sz / sizeof(uint32_t);
	uint32_t *u32 = addr;

	le = structure_endian_prefer_le(le);

	if (!array || !cJSON_IsArray(array))
		return -1;

	cJSON_ArrayForEach(item, array) {
		const char *s = item->valuestring;
		uint32_t val;
		int err = 0;

		if (!(i < max_items))
			break;

		val = (uint32_t)strict_strtol(s, base, &err, NULL);
		if (err)
			return -1;

		u32[i++] = le ? le32_to_cpu(val) : be32_to_cpu(val);
	}

	return 0;
}

int structure_item_save_json_x32_array(cJSON *json, const char *name,
				       const void *addr, size_t sz)
{
	return _structure_item_save_json_n32_array(json, name, 1, "0x%08x",
						   addr, sz);
}

int structure_item_save_json_x32_be_array(cJSON *json, const char *name,
					  const void *addr, size_t sz)
{
	return _structure_item_save_json_n32_array(json, name, 0, "0x%08x",
						   addr, sz);
}

int structure_item_save_json_u32_array(cJSON *json, const char *name,
				       const void *addr, size_t sz)
{
	return _structure_item_save_json_n32_array(json, name, 1, "%d",
						   addr, sz);
}

int structure_item_save_json_u32_be_array(cJSON *json, const char *name,
					  const void *addr, size_t sz)
{
	return _structure_item_save_json_n32_array(json, name, 0, "%d",
						   addr, sz);
}

int structure_item_load_json_x32_array(cJSON *json, const char *name,
				       void *addr, size_t sz)
{
	return _structure_item_load_json_n32_array(json, name, 1, 16, addr, sz);
}

int structure_item_load_json_x32_be_array(cJSON *json, const char *name,
					  void *addr, size_t sz)
{
	return _structure_item_load_json_n32_array(json, name, 0, 16, addr, sz);
}

int structure_item_load_json_u32_array(cJSON *json, const char *name,
				       void *addr, size_t sz)
{
	return _structure_item_load_json_n32_array(json, name, 1, 10, addr, sz);
}

int structure_item_load_json_u32_be_array(cJSON *json, const char *name,
					  void *addr, size_t sz)
{
	return _structure_item_load_json_n32_array(json, name, 0, 10, addr, sz);
}

static void _structure_item_print_n32_array(const char *print_name_fmt,
					    const char *name,
					    int le,
					    const char *fmt,
					    const void *addr, size_t sz)
{
	const uint32_t *p_u32 = addr;

	le = structure_endian_prefer_le(le);

	structure_print_name(print_name_fmt, name);
	for (size_t i = 0; i < sz / sizeof(uint32_t); i++)
		printf(fmt, le ? le32_to_cpu(p_u32[i]) : be32_to_cpu(p_u32[i]));
	printf("\n");
}

static void _structure_item_print_x32_array(const char *print_name_fmt,
					    const char *name,
					    int le,
					    const void *addr, size_t sz)
{
	_structure_item_print_n32_array(print_name_fmt,
					name,
					le,
					"0x%08x ",
					addr, sz);
}

void structure_item_print_x32_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_x32_array(print_name_fmt, name, 1, addr, sz);
}

void structure_item_print_be_x32_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_x32_array(print_name_fmt, name, 0, addr, sz);
}

static void _structure_item_print_u32_array(const char *print_name_fmt,
					    const char *name,
					    int le,
					    const void *addr, size_t sz)
{
	_structure_item_print_n32_array(print_name_fmt,
					name,
					le,
					"%u ",
					addr, sz);
}

void structure_item_print_u32_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_u32_array(print_name_fmt, name, 1, addr, sz);
}

void structure_item_print_be_u32_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_u32_array(print_name_fmt, name, 0, addr, sz);
}

static void _structure_item_print_n16_array(const char *print_name_fmt,
					    const char *name,
					    int le,
					    const char *fmt,
					    const void *addr, size_t sz)
{
	const uint16_t *p_u16 = addr;

	le = structure_endian_prefer_le(le);

	structure_print_name(print_name_fmt, name);
	for (size_t i = 0; i < sz / sizeof(uint16_t); i++)
		printf(fmt, le ? le16_to_cpu(p_u16[i]) : be16_to_cpu(p_u16[i]));
	printf("\n");
}

static void _structure_item_print_x16_array(const char *print_name_fmt,
					    const char *name,
					    int le,
					    const void *addr, size_t sz)
{
	_structure_item_print_n16_array(print_name_fmt,
					name,
					le,
					"0x%04x ",
					addr, sz);
}

void structure_item_print_x16_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_x16_array(print_name_fmt, name, 1, addr, sz);
}

void structure_item_print_be_x16_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_x16_array(print_name_fmt, name, 0, addr, sz);
}

static void _structure_item_print_u16_array(const char *print_name_fmt,
					    const char *name,
					    int le,
					    const void *addr, size_t sz)
{
	_structure_item_print_n16_array(print_name_fmt,
					name,
					le,
					"%u ",
					addr, sz);
}

void structure_item_print_u16_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_u16_array(print_name_fmt, name, 1, addr, sz);
}

void structure_item_print_be_u16_array(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz)
{
	_structure_item_print_u16_array(print_name_fmt, name, 0, addr, sz);
}

void structure_item_print_string(const char *print_name_fmt, const char *name,
				 const void *addr, size_t sz)
{
	const char *p = addr;

	structure_print_name(print_name_fmt, name);
	for (size_t i = 0; i < sz && p[i] != '\0'; i++)
		putchar(p[i]);
	printf("\n");
}

void structure_item_print_hexdump(const char *print_name_fmt, const char *name,
				  const void *addr, size_t sz)
{
	structure_print_name(print_name_fmt, name);
	hexdump_indent(print_name_fmt, addr, sz, 0);
}

int structure_item_load_json_string(cJSON *json, const char *name,
				    void *addr, size_t sz)
{
	const char *s = json_get_string_value_in_object(json, name);

	if (!s)
		return -1;

	snprintf(addr, sz, "%s", s);
	return 0;
}

int structure_item_save_json_string(cJSON *json, const char *name,
				    const void *addr, size_t sz)
{
	char tmps[sz + 1];

	/* make sure the buffer is ending with '\0' */
	memcpy(tmps, addr, sz);
	tmps[sz] = '\0';

	json_create_or_update_string_value(json, name, tmps);
	return 0;
}

void structure_item_print_unix_epoch(const char *print_name_fmt, const char *name,
				     const void *addr, size_t sz)
{
	time_t t = le32_to_cpu(*((const __le32 *)addr));
	struct tm tm;
	char buf[64];

	structure_print_name(print_name_fmt, name);
	localtime_r(&t, &tm);
	strftime(buf, sizeof(buf), "%FT%T", &tm);
	printf("%s\n", buf);
}

void structure_print(const char *print_name_fmt, const void *base,
		     const struct structure_item *items)
{
	for (const struct structure_item *item = items; item->name; item++) {
		if (item->print)
			item->print(print_name_fmt, item->name,
				    base + item->offset,
				    item->size);
	}
}

typedef void (*print_t)(const char *, const char *, const void *, size_t);
typedef int (*save_json_t)(cJSON *, const char *, const void *, size_t);
typedef int (*load_json_t)(cJSON *, const char *, void *, size_t);

struct structure_item_type {
	print_t		print;
	save_json_t	save_json;
	load_json_t	load_json;
};

#define STRUCTURE_ITEM_TYPE(t) {			\
	.print = structure_item_print_##t,		\
	.save_json = structure_item_save_json_##t,	\
	.load_json = structure_item_load_json_##t,	\
}

static const struct structure_item_type available_types[] = {
	STRUCTURE_ITEM_TYPE(string),
	STRUCTURE_ITEM_TYPE(unsigned),
	STRUCTURE_ITEM_TYPE(be_unsigned),
	STRUCTURE_ITEM_TYPE(xunsigned),
	STRUCTURE_ITEM_TYPE(be_xunsigned),
	STRUCTURE_ITEM_TYPE(x8_array),
	STRUCTURE_ITEM_TYPE(x32_array),
	{ NULL }, /* ending */
};

static const struct structure_item_type *get_structure_item_type(print_t print)
{
	const struct structure_item_type *t = NULL;

	if (print) {
		for (size_t i = 0; available_types[i].print; i++) {
			if (available_types[i].print == print) {
				t = &available_types[i];
				break;
			}
		}
	}

	return t;
}

int structure_save_json(cJSON *json, const void *base,
			const struct structure_item *items)
{
	for (const struct structure_item *item = items; item->name; item++) {
		save_json_t save = item->save_json;
		int ret;

		if (item->flags & STRUCTURE_FLAG_NOT_SAVE)
			continue;

		if (!save) { /* auto detect the save functions */
			const struct structure_item_type *t =
					get_structure_item_type(item->print);
			if (t)
				save = t->save_json;
		}

		if (!save) {
			fprintf(stderr, "Error: %s: save %s failed"
				"(can't detect the method)\n",
				__func__, item->name);
			return -1;
		}

		ret = save(json, item->name, base + item->offset, item->size);
		if (ret < 0) {
			fprintf(stderr, "Error: %s: save %s failed\n",
				__func__, item->name);
			return ret;
		}
	}

	return 0;
}

int structure_load_json(cJSON *json, void *base,
			const struct structure_item *items)
{
	for (const struct structure_item *item = items; item->name; item++) {
		load_json_t load = item->load_json;
		int ret;

		if (item->flags & STRUCTURE_FLAG_NOT_SAVE)
			continue;

		if (!load) {
			/* auto detect the load functions */
			const struct structure_item_type *t =
					get_structure_item_type(item->print);
			if (t)
				load = t->load_json;
		}

		if (!load) {
			fprintf(stderr, "Error: %s: load %s failed"
				"(can't detect the method)\n",
				__func__, item->name);
			return -1;
		}

		ret = load(json, item->name, base + item->offset, item->size);
		if (ret < 0) {
			fprintf(stderr, "Error: %s: load %s failed\n",
				__func__, item->name);
			return ret;
		}
	}

	return 0;
}

