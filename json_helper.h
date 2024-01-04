/*
 * json helper library
 *
 * qianfan Zhao
 */
#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include <cJSON.h>

cJSON *json_from_file(const char *name);
int json_saveto_file(cJSON *json, const char *filename);

cJSON *json_get_object_bypath(cJSON *top, const char *path);

const char *json_get_string_value_in_object(cJSON *json, const char *id);
char *json_copy_string_value_in_object(cJSON *json, const char *id,
				       char *dst, size_t sz);
const char *json_get_string_value_bypath(cJSON *top, const char *path);
long long json_get_ll_value_bypath(cJSON *top, const char *path, int *err);
long long json_get_ll_value_bypath_or_default(cJSON *top, const char *path, long long def);
long json_string_value_to_number(cJSON *json, const char *id, int base, int *err);
long json_string_value_to_number_or_default(cJSON *json, const char *id,
					    int base, long defval);

long json_string_value_to_number_bypath_or_default(cJSON *json,
						   const char *path,
						   int base,
						   long defval);

#define json_add_string_value_or_goto_fail(root, name, value) do {	\
	if ((value) != NULL) {						\
		if (!(root))						\
			goto fail;					\
		if (!cJSON_AddStringToObject(root, (name), (value)))	\
			goto fail;					\
	}								\
} while (0)

int json_add_string_value(cJSON *root, const char *name, const char *value);
int __attribute__((format(printf, 3, 4)))
	json_format_add_string_value(cJSON *root, const char *name, const char *fmt, ...);
cJSON *json_create_or_update_string_value(cJSON *root, const char *name,
					  const char *val);
cJSON *json_array_push_stringvalue(cJSON *root, const char *array_path,
				   const char *val);

void *json_loading_arrays(cJSON *arrays, int (*load)(cJSON *,  void *),
			  size_t persize, size_t *count);
void *json_loading_arrays_from_object(cJSON *root, const char *id,
				      int (*load)(cJSON *, void *),
				      size_t persize, size_t *count);

/*
 * Return true if @matchstr is in @string_arrays
 */
int json_string_array_object_is_match(cJSON *string_arrays, const char *matchstr);
int json_string_array_is_match(cJSON *root, const char *path, const char *matchstr);

#endif

