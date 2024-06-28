/**
 * XOpt - command line parsing library
 *
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef LIBXOPT_H
#define LIBXOPT_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>	/* import offsetof */

enum xopt_type {
	XOPT_TYPE_BOOL = 1,
	XOPT_TYPE_STRING,
	XOPT_TYPE_INT,
	XOPT_TYPE_INT_DEC,
	XOPT_TYPE_INT_HEX,
	XOPT_TYPE_LONG,
	XOPT_TYPE_LONG_DEC,
	XOPT_TYPE_LONG_HEX,
	XOPT_TYPE_ULONG,
	XOPT_TYPE_ULONG_DEC,
	XOPT_TYPE_ULONG_HEX,
};

struct xopt;
struct xopt_option;
typedef int (*xopt_option_callback_t)(struct xopt *, const struct xopt_option *,
				      bool is_shortarg,
				      const char *s,
				      void *data);

struct xopt_option {
	char				name;
	const char			*long_name;
	enum xopt_type			type;
	size_t				offset;
	xopt_option_callback_t		callback;
};

#define LIBXOPT_NULLOPTION 		{ '\0', NULL }

#define LIBXOPT_FLAG_KEEPFIRST		(1 << 0)

struct xopt *libxopt_new(const struct xopt_option *ops, unsigned long flags);
void libxopt_reinit(struct xopt *xopt);
void libxopt_free(struct xopt *xopt);

void libxopt_set_error(struct xopt *, const char *, ...);
const char *libxopt_get_error(struct xopt *);

int libxopt_parse(struct xopt *, int argc, char **argv, void *);
int libxopt_get_optind(struct xopt *);

#endif
