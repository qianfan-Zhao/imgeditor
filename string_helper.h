/*
 * string helper library
 * qianfan Zhao
 */

#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include <string.h>
#include <ctype.h>

static inline size_t string_count_char(const char *s, char c)
{
	size_t n = 0;

	for (; *s != '\0'; s++) {
		if (*s == c)
			++n;
	}

	return n;
}

static inline size_t string_count_space(const char *s)
{
	size_t n = 0;

	for (; *s != '\0'; s++) {
		if (isspace(*s))
			++n;
	}

	return n;
}

static inline char *string_skip_head_space(char *s)
{
	for ( ; *s != '\0' && isspace(*s); s++) {
	}

	return s;
}

/* remove end of line space */
static inline char *string_remove_eol_space(char *s)
{
	char *p = s + strlen(s) - 1;

	if (*s != '\0') {
		for (; p >= s && isspace(*p); p--) {
		}
		p[1] = '\0';
	}

	return s;
}

#define static_strlen(s)	(sizeof(s) - sizeof(""))

int string_replace_in_buffer(char *buffer, size_t bufsz, const char *token,
			     const char *to);

long strict_strtol(const char *s, int base, int *err, const char *tail);
long strict_strtol_or_default(const char *s, int base, const char *tail, long defv);
long long strict_strtoll(const char *s, int base, int *err, const char *tail);
long long strict_strtoll_or_default(const char *s, int base, const char *tail, long long defv);

unsigned long strict_strtoul(const char *s, int base, int *err, const char *tail);
unsigned long strict_strtoul_or_default(const char *s, int base, const char *tail, unsigned long defv);
unsigned long long strict_strtoull(const char *s, int base, int *err, const char *tail);
unsigned long long strict_strtoull_or_default(const char *s, int base, const char *tail, unsigned long long defv);

__attribute__((format(printf, 3, 4)))
size_t strncatprintf(char *s, size_t sz, const char *fmt, ...);

__attribute__((format(printf, 3, 4)))
size_t snprintf_continue(char **s, size_t *sz, const char *fmt, ...);

ssize_t argv_buffer_split_with_delim(char *cmdlines, const char *delim,
				     char **argv, size_t sz_argv);

#endif

