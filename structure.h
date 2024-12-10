/*
 * structure helper library
 * qianfan Zhao 2022-11-11
 */
#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <stddef.h>
#include <cJSON.h>

#define STRUCTURE_FLAG_NOT_SAVE			(1 << 0)

struct structure_item {
	const char	*name;
	size_t		offset;
	size_t		size;
	void		(*print)(const char *print_name_fmt, const char *name,
				 const void *addr, size_t sz);
	int		(*save_json)(cJSON *json, const char *name,
				     const void *addr, size_t sz);
	int		(*load_json)(cJSON *json, const char *name,
				     void *addr, size_t sz);
	unsigned int	flags;
};

#define STRUCTURE_ITEM(_type, _name, _print, ...) {			\
	.name = #_name,							\
	.offset = offsetof(_type, _name),				\
	.size = sizeof(((_type *)0)->_name),				\
	.print = _print,						\
	##__VA_ARGS__							\
}

#define STRUCTURE_ITEM_END() {						\
	.name = NULL,							\
}

enum structure_endian {
	STRUCTURE_ENDIAN_FORCE_NONE,
	STRUCTURE_ENDIAN_FORCE_LE,
	STRUCTURE_ENDIAN_FORCE_BE,
};

enum structure_endian structure_force_endian(enum structure_endian set);

void structure_print_name(const char *print_name_fmt, const char *name);

void structure_item_print_unsigned(const char *print_name_fmt, const char *name,
				   const void *addr, size_t sz);
void structure_item_print_be_unsigned(const char *print_name_fmt, const char *name,
				      const void *addr, size_t sz);
void structure_item_print_xunsigned(const char *print_name_fmt, const char *name,
				    const void *addr, size_t sz);
void structure_item_print_be_xunsigned(const char *print_name_fmt, const char *name,
				       const void *addr, size_t sz);
void structure_item_print_x8_array(const char *print_name_fmt, const char *name,
				   const void *addr, size_t sz);
void structure_item_print_x16_array(const char *print_name_fmt, const char *name,
				    const void *addr, size_t sz);
void structure_item_print_be_x16_array(const char *print_name_fmt, const char *name,
				       const void *addr, size_t sz);
void structure_item_print_u16_array(const char *print_name_fmt, const char *name,
				    const void *addr, size_t sz);
void structure_item_print_be_u16_array(const char *print_name_fmt, const char *name,
				       const void *addr, size_t sz);
void structure_item_print_x32_array(const char *print_name_fmt, const char *name,
				    const void *addr, size_t sz);
void structure_item_print_be_x32_array(const char *print_name_fmt, const char *name,
				       const void *addr, size_t sz);
void structure_item_print_u32_array(const char *print_name_fmt, const char *name,
				    const void *addr, size_t sz);
void structure_item_print_be_u32_array(const char *print_name_fmt, const char *name,
				       const void *addr, size_t sz);
void structure_item_print_string(const char *print_name_fmt, const char *name,
				 const void *addr, size_t sz);
void structure_item_print_unix_epoch(const char *print_name_fmt, const char *name,
				     const void *addr, size_t sz);
void structure_item_print_hexdump(const char *print_name_fmt, const char *name,
				  const void *addr, size_t sz);

void structure_print(const char *print_name_fmt, const void *base,
		     const struct structure_item *items);

int structure_item_save_json_unsigned(cJSON *json, const char *name,
				      const void *addr, size_t sz);
int structure_item_save_json_be_unsigned(cJSON *json, const char *name,
					 const void *addr, size_t sz);
int structure_item_save_json_xunsigned(cJSON *json, const char *name,
				       const void *addr, size_t sz);
int structure_item_save_json_be_xunsigned(cJSON *json, const char *name,
					  const void *addr, size_t sz);
int structure_item_save_json_x8_array(cJSON *json, const char *name,
				      const void *addr, size_t sz);
int structure_item_save_json_x32_array(cJSON *json, const char *name,
				       const void *addr, size_t sz);
int structure_item_save_json_x32_be_array(cJSON *json, const char *name,
					  const void *addr, size_t sz);
int structure_item_save_json_u32_array(cJSON *json, const char *name,
				       const void *addr, size_t sz);
int structure_item_save_json_u32_be_array(cJSON *json, const char *name,
					  const void *addr, size_t sz);

int structure_save_json(cJSON *json, const void *base,
			const struct structure_item *items);

int structure_item_load_json_unsigned(cJSON *json, const char *name,
				      void *addr, size_t sz);
int structure_item_load_json_xunsigned(cJSON *json, const char *name,
				       void *addr, size_t sz);
int structure_item_load_json_be_unsigned(cJSON *json, const char *name,
					 void *addr, size_t sz);
int structure_item_load_json_be_xunsigned(cJSON *json, const char *name,
					  void *addr, size_t sz);
int structure_item_load_json_x8_array(cJSON *json, const char *name,
				      void *addr, size_t sz);

int structure_item_load_json_x32_array(cJSON *json, const char *name,
				       void *addr, size_t sz);
int structure_item_load_json_x32_be_array(cJSON *json, const char *name,
					  void *addr, size_t sz);
int structure_item_load_json_u32_array(cJSON *json, const char *name,
				       void *addr, size_t sz);
int structure_item_load_json_u32_be_array(cJSON *json, const char *name,
					  void *addr, size_t sz);

int structure_load_json(cJSON *json, void *base,
			const struct structure_item *items);

/* bit flags helper functions */
struct structure_bit_descriptor {
	unsigned long		value;
	const char		*descriptor;
};

void structure_item_print_bit_flags(const char *print_name_fmt,
				    const char *name,
				    const void *addr, size_t sz,
				    const struct structure_bit_descriptor *d);

void structure_item_print_be_bit_flags(const char *print_name_fmt,
				       const char *name,
				       const void *addr, size_t sz,
				       const struct structure_bit_descriptor *d);

void structure_item_print_enum(const char *print_name_fmt, const char *name,
			       const void *addr, size_t sz,
			       unsigned rshift, unsigned long mask,
			       const struct structure_bit_descriptor *d);

void structure_item_print_be_enum(const char *print_name_fmt, const char *name,
				  const void *addr, size_t sz,
				  unsigned rshift, unsigned long mask,
				  const struct structure_bit_descriptor *d);

#endif
