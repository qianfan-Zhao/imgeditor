#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

int string_replace_in_buffer(char *buffer, size_t bufsz, const char *token,
			     const char *to)
{
	size_t l_token = strlen(token), l_to = strlen(to);
	char *s = buffer;

	while (1) {
		size_t remain = 0;

		s = strstr(s, token);
		if (!s)
			break;

		remain = strlen(s + l_token);

		if (l_token >= l_to) {
			/* xxxbcdefxxx: replace "bcdef" to "b"
			 *    |---|
			 *    token
			 */
			memmove(s, to, l_to);
			memmove(s + l_to, s + l_token, remain);
		} else /* l_token < l_to */ {
			/* xxxbxxx: replace "b" to "bcdef" */
			size_t expand = l_to - l_token, used = s - buffer;

			if (expand + used + 1 >= bufsz) /* @bufsz limited */
				return -1;

			memmove(s + l_to, s + l_token, remain); /* expand first */
			memmove(s, to, l_to);
		}

		s += l_to;
		s[remain] = '\0';
	}

	return 0;
}

#define strict_strto(type, func)					\
type strict_##func(const char *s, int base, int *err, const char *tail)	\
{									\
	type n = 0;							\
	char *endp;							\
									\
	*err = 1;							\
	n = func(s, &endp, base);					\
	if (tail) {							\
		for (const char *s = &tail[0]; *s; s++) {		\
			if (*endp == *s) {				\
				*err = 0;				\
				break;					\
			}						\
		}							\
	} else if (*endp == '\0') {					\
		*err = 0;						\
	}								\
									\
	return n;							\
}

#define strict_strto_or_default(type, func)				\
type strict_##func##_or_default(const char *s, int base,	 	\
				const char *tail, type defv)		\
{									\
	type n;								\
	int err;							\
									\
	n = strict_##func(s, base, &err, tail);				\
	if (err)							\
		n = defv;						\
	return n;							\
}

typedef long long long_long_t;

strict_strto(long long, strtoll);
strict_strto_or_default(long long, strtoll);
strict_strto(long, strtol);
strict_strto_or_default(long, strtol);

strict_strto(unsigned long, strtoul);
strict_strto_or_default(unsigned long, strtoul);
strict_strto(unsigned long long, strtoull);
strict_strto_or_default(unsigned long long, strtoull);

__attribute__((format(printf, 3, 4)))
size_t strncatprintf(char *s, size_t sz, const char *fmt, ...)
{
	size_t n, used = 0;
	va_list arg;

	while (*s != '\0') {
		s++;
		used++;
		sz--;
	}

	va_start(arg, fmt);
	n = vsnprintf(s, sz, fmt, arg);
	va_end(arg);

	return used + n;
}

__attribute__((format(printf, 3, 4)))
size_t snprintf_continue(char **s, size_t *sz, const char *fmt, ...)
{
	size_t n, written, freesz = *sz;
	va_list arg;

	va_start(arg, fmt);
	n = vsnprintf(*s, freesz, fmt, arg);
	va_end(arg);

	if (n >= freesz) {
		/* sz limited, only (freesz - 1) bytes is written */
		written = freesz - 1;
	} else {
		written = n;
	}

	*s = *s + written;
	*sz -= written;

	return n;
}

ssize_t argv_buffer_split_with_delim(char *cmdlines, const char *delim,
				     char **argv, size_t sz_argv)
{
	char *s, *save_ptr;
	size_t argc = 0;

	if (!cmdlines || !delim || !argv || sz_argv == 0)
		return 0;

	s = strtok_r(cmdlines, delim, &save_ptr);
	if (!s)
		goto done;
	argv[argc++] = s;

	while (argc < sz_argv - 1) {
		s = strtok_r(NULL, delim, &save_ptr);
		if (!s)
			break;
		argv[argc++] = s;
	}

done:
	argv[argc] = NULL;
	return argc;
}
