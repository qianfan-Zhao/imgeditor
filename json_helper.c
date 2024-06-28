/*
 * json helper library for cJSON
 *
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "json_helper.h"

cJSON *json_get_object_bypath(cJSON *top, const char *path)
{
	char nonconst_path[128] = { 0 };
	char *p, *next, *slash;
	cJSON *json = top;

	strncpy(nonconst_path, path, sizeof(nonconst_path) - 1);
	p = nonconst_path;

	if (p[0] == '/')
		p++;

	do {
		if ((slash = strchr(p, '/'))) {
			*slash = '\0';
			next = slash + 1;
		} else {
			next = NULL;
		}

		json = cJSON_GetObjectItem(json, p);
		if (!json)
			break;

		p = next;
	} while (next);

	return json;
}

const char *json_get_string_value_bypath(cJSON *top, const char *path)
{
	cJSON *json = json_get_object_bypath(top, path);

	if (json && cJSON_IsString(json))
		return cJSON_GetStringValue(json);
	else if (json)
		fprintf(stderr, "%s is not StringValue\n", path);

	return NULL;
}

long long json_get_ll_value_bypath(cJSON *top, const char *path, int *err)
{
	cJSON *json = json_get_object_bypath(top, path);
	long long v = 0;

	if (json && cJSON_IsNumber(json)) {
		v = (long long)cJSON_GetNumberValue(json);
		*err = 0;
	} else {
		*err = 1;
	}

	return v;
}

long long json_get_ll_value_bypath_or_default(cJSON *top, const char *path, long long def)
{
	int err;
	long long v;

	v = json_get_ll_value_bypath(top, path, &err);
	if (err)
		v = def;

	return v;
}

const char *json_get_string_value_in_object(cJSON *json, const char *id)
{
	cJSON *s = cJSON_GetObjectItem(json, id);

	if (s && cJSON_IsString(s))
		return cJSON_GetStringValue(s);

	return NULL;
}

char *json_copy_string_value_in_object(cJSON *json, const char *id,
				       char *dst, size_t sz)
{
	const char *s = json_get_string_value_in_object(json, id);

	if (!s)
		return NULL;

	snprintf(dst, sz, "%s", s);
	return dst;
}

static long string_value_to_number(const char *s, int base, int *err)
{
	long value = 0;
	char *endp;

	if (err)
		*err = 1;

	if (s) {
		value = strtol(s, &endp, base);
		if (*endp == '\0') {
			if (err)
				*err = 0;
		}
	}

	return value;
}

long json_string_value_to_number(cJSON *json, const char *id, int base, int *err)
{
	const char *s = json_get_string_value_in_object(json, id);

	return string_value_to_number(s, base, err);
}

long json_string_value_to_number_or_default(cJSON *json, const char *id,
					    int base, long defval)
{
	long value;
	int err;

	value = json_string_value_to_number(json, id, base, &err);
	if (err)
		value = defval;

	return value;
}

long json_string_value_to_number_bypath_or_default(cJSON *root,
						   const char *path,
						   int base,
						   long defval)
{
	cJSON *json = json_get_object_bypath(root, path);
	const char *s = cJSON_GetStringValue(json);
	int err = 1;
	long v;

	v = string_value_to_number(s, base, &err);
	if (err)
		v = defval;

	return v;
}

int json_add_string_value(cJSON *root, const char *name, const char *value)
{
	int ret = -1;

	json_add_string_value_or_goto_fail(root, name, value);
	ret = 0;
fail:
	return ret;
}

int json_format_add_string_value(cJSON *root, const char *name, const char *fmt, ...)
{
	char s[256] = { 0 };
	va_list arg;

	va_start(arg, fmt);
	vsnprintf(s, sizeof(s), fmt, arg);
	va_end(arg);

	return json_add_string_value(root, name, s);
}

static char *loading_file(const char *f, size_t *size)
{
	FILE *fp = fopen(f, "rb");
	char *buf = NULL;
	long length;

	if (fp) {
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);

		if (length > 0) {
			fseek(fp, 0, SEEK_SET);
			buf = malloc(length);
			if (buf)
				*size = fread(buf, 1, length, fp);
		}
		fclose(fp);
	} else {
		fprintf(stderr, "loading %s failed: %m\n", f);
	}

	return buf;
}

cJSON *json_from_file(const char *filename)
{
	cJSON *json = NULL;
	size_t sz = 0;
	char *s;

	s = loading_file(filename, &sz);
	if (s) {
		json = cJSON_ParseWithLength(s, sz);
		if (!json)
			fprintf(stderr, "parse %s failed when: %s\n",
				filename, cJSON_GetErrorPtr());
		free(s);
	}

	return json;
}

int json_saveto_file(cJSON *json, const char *filename)
{
	FILE *fp = fopen(filename, "w+");
	int ret = -1;
	char *s;

	if (!fp) {
		fprintf(stderr, "open %s for write failed: %m\n", filename);
		return ret;
	}

	s = cJSON_Print(json);
	if (s) {
		fprintf(fp, "%s", s);
		cJSON_free(s);
		ret = 0;
	}

	fclose(fp);
	return ret;
}

/*
 * create or updating string value
 */
cJSON *json_create_or_update_string_value(cJSON *root, const char *name,
					  const char *val)
{
	cJSON *json = cJSON_GetObjectItem(root, name);

	if (!json) {
		json = cJSON_AddStringToObject(root, name, val);
		if (!json) {
			fprintf(stderr, "%s: ceate string <%s> failed\n",
				__func__, name);
			return json;
		}
	} else {
		if (!cJSON_IsString(json)) {
			fprintf(stderr, "%s: <%s> is not string value\n",
				__func__, name);
			return NULL;
		}

		if (strcmp(cJSON_GetStringValue(json), val) != 0)
			cJSON_SetValuestring(json, val);
	}

	return json;
}

cJSON *json_array_push_stringvalue(cJSON *root, const char *array_path,
				   const char *val)
{
	cJSON *array = json_get_object_bypath(root, array_path);
	cJSON *string;

	if (!array)
		return NULL;

	/* maybe the string value is already pushed */
	cJSON_ArrayForEach(string, array) {
		if (!strcmp(val, cJSON_GetStringValue(string)))
			return string;
	}

	/* not found, create it */
	string = cJSON_CreateString(val);
	if (!string)
		return string;

	cJSON_AddItemToArray(array, string);
	return string;
}

static int _json_string_array_object_index_of(cJSON *string_arrays,
					      const char *matchstr,
					      int (*compare)(const char *, const char *))
{
	int i = 0;
	cJSON *json;

	cJSON_ArrayForEach(json, string_arrays) {
		if (compare(matchstr, cJSON_GetStringValue(json)) == 0)
			return i;
		i++;
	}

	return -1; /* Not found */
}

static int json_string_array_object_index_of(cJSON *string_arrays,
					     const char *matchstr)
{
	return _json_string_array_object_index_of(string_arrays, matchstr, strcmp);
}

int json_string_array_object_is_match(cJSON *string_arrays, const char *matchstr)
{
	return ! (json_string_array_object_index_of(string_arrays, matchstr) < 0);
}

int json_string_array_is_match(cJSON *root, const char *path, const char *matchstr)
{
	cJSON *array = json_get_object_bypath(root, path);

	return cJSON_IsArray(array) &&
		json_string_array_object_is_match(array, matchstr);
}

/*
 * alloc memory and loading json arrays to structure, the byte size of each
 * structure is @persize.
 */
void *json_loading_arrays(cJSON *arrays, int (*load)(cJSON *,  void *),
			  size_t persize, size_t *count)
{
	int arraysize;
	void *mem;

	*count = 0;
	if (!cJSON_IsArray(arrays))
		return NULL;

	arraysize = cJSON_GetArraySize(arrays);
	if (arraysize <= 0)
		return NULL;

	mem = calloc(arraysize, persize);
	if (!mem)
		return mem;

	size_t n = 0;
	cJSON *json;

	cJSON_ArrayForEach(json, arrays) {
		if (load(json, mem + n * persize) < 0) {
			free(mem);
			mem = NULL;
			n = 0;
			break;
		}
		n++;
	}

	*count = n;
	return mem;
}

void *json_loading_arrays_from_object(cJSON *root, const char *id,
				      int (*load)(cJSON *, void *),
				      size_t persize, size_t *count)
{
	cJSON *json = cJSON_GetObjectItem(root, id);

	if (json && cJSON_IsArray(json))
		return json_loading_arrays(json, load, persize, count);

	return NULL;
}
