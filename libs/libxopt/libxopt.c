/*
 * XOpt - command line parsing library
 *
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <getopt.h>
#include "libxopt.h"

#define XOPT_GETOPT_STARTVAL           512

struct xopt {
	unsigned long			flags;

	const struct xopt_option	*options;
	size_t				option_counts;

	/* auto generated for getopt_long */
	char				optstring[128];
	struct option			*longops;

	char				errbuf[1024];
};

static int xopt_push_optstring(struct xopt *xopt, size_t *short_idx,
			       char name, int has_arg)
{
	size_t need_space = 1 /* null */ + 1 /* name */ + !!has_arg;
	size_t idx = *short_idx;
	size_t remain = sizeof(xopt->optstring) - idx;

	if (remain < need_space)
		return -1;

	xopt->optstring[idx++] = name;
	if (has_arg)
		xopt->optstring[idx++] = ':';
	xopt->optstring[idx] = '\0';

	*short_idx = idx;
	return 0;
}

static int xopt_push_long_option(struct xopt *xopt, size_t *long_idx,
				 const char *long_name, int has_arg,
				 int val)
{
	struct option *opt = &xopt->longops[*long_idx];

	opt->name = long_name;
	opt->has_arg = has_arg ? required_argument : no_argument;
	opt->flag = NULL;
	opt->val = val;

	(*long_idx)++;
	return 0;
}

static const struct xopt_option *xopt_option_get_byname(struct xopt *xopt,
							char name)
{
	for (size_t i = 0; i < xopt->option_counts; i++) {
		const struct xopt_option *opt = &xopt->options[i];

		if (opt->name == name)
			return opt;
	}

	return NULL;
}

static const struct xopt_option *xopt_option_get_byval(struct xopt *xopt,
						       int val)
{
	if (val < XOPT_GETOPT_STARTVAL)
		return NULL;
	if (val >= XOPT_GETOPT_STARTVAL + (int)xopt->option_counts)
		return NULL;

	return &xopt->options[val - XOPT_GETOPT_STARTVAL];
}

static size_t xopt_option_counts(const struct xopt_option *options)
{
	const struct xopt_option *opt = options;
	size_t n = 0;

	for (; opt->name != '\0' || opt->long_name; opt++)
		n++;

	return n;
}

struct xopt *libxopt_new(const struct xopt_option *options,
			 unsigned long flags)
{
	struct xopt *xopt = calloc(1, sizeof(*xopt));
	size_t long_idx = 0, short_idx = 0;
	int ret = 0;

	if (!xopt)
		return xopt;

	xopt->flags = flags;
	memset(xopt->errbuf, 0, sizeof(xopt->errbuf));

	xopt->options = options;
	xopt->option_counts = xopt_option_counts(xopt->options);
	xopt->longops = calloc(xopt->option_counts + 1,
				sizeof(xopt->longops[0]));
	if (!xopt->longops) {
		free(xopt);
		return NULL;
	}

	for (size_t i = 0; i < xopt->option_counts; i++) {
		const struct xopt_option *xoption = &xopt->options[i];
		int has_arg = xoption->type != XOPT_TYPE_BOOL;
		int val = XOPT_GETOPT_STARTVAL + i;

		if (xoption->name != '\0')
			ret = xopt_push_optstring(xopt, &short_idx,
						  xoption->name, has_arg);
		if (xoption->long_name)
			ret = xopt_push_long_option(xopt, &long_idx,
						    xoption->long_name,
						    has_arg,
						    val);

		if (ret < 0) {
			free(xopt->longops);
			free(xopt);
			return NULL;
		}
	}

	libxopt_reinit(xopt);

	return xopt;
}

void libxopt_free(struct xopt *xopt)
{
	free(xopt->longops);
	free(xopt);
}

void libxopt_reinit(struct xopt *xopt)
{
	optind = 0;

	memset(xopt->errbuf, 0, sizeof(xopt->errbuf));
}

void libxopt_set_error(struct xopt *xopt, const char *fmt, ...)
{
	va_list list;

	va_start(list, fmt);
	vsnprintf(xopt->errbuf, sizeof(xopt->errbuf), fmt, list);
	va_end(list);
}

const char *libxopt_get_error(struct xopt *xopt)
{
	return xopt->errbuf;
}

static int xopt_default_callback(struct xopt *xopt,
				 const struct xopt_option *option,
				 bool is_shortarg, const char *value,
				 void *data_base)
{
	void *target = data_base + option->offset;
	char *endp = NULL;
	int base = 0;

	switch (option->type) {
	case XOPT_TYPE_INT_DEC:
	case XOPT_TYPE_LONG_DEC:
	case XOPT_TYPE_ULONG_DEC:
		base = 10;
		break;
	case XOPT_TYPE_INT_HEX:
	case XOPT_TYPE_LONG_HEX:
	case XOPT_TYPE_ULONG_HEX:
		base = 16;
		break;
	default:
		break;
	}

	switch (option->type) {
	case XOPT_TYPE_BOOL:
		*((bool *)target) = true;
		break;
	case XOPT_TYPE_STRING:
		/* lifetime here works out fine; argv can usually be assumed static-like
			 in nature */
		*((const char **)target) = value;
		break;
	case XOPT_TYPE_INT:
	case XOPT_TYPE_INT_DEC:
	case XOPT_TYPE_INT_HEX:
		*((int *)target) = (int)strtol(value, &endp, base);
		break;
	case XOPT_TYPE_LONG:
	case XOPT_TYPE_LONG_DEC:
	case XOPT_TYPE_LONG_HEX:
		*((long *)target) = strtol(value, &endp, base);
		break;
	case XOPT_TYPE_ULONG:
	case XOPT_TYPE_ULONG_DEC:
	case XOPT_TYPE_ULONG_HEX:
		*((unsigned long *)target) = strtoul(value, &endp, base);
		break;
	}

	if (endp && *endp != '\0') {
		#define NOTNUMBER "value isn't a valid number: "
		if (is_shortarg)
			libxopt_set_error(xopt, NOTNUMBER "-%c %s",
					 option->name, value);
		else
			libxopt_set_error(xopt, NOTNUMBER "--%s %s",
					  option->long_name, value);
		#undef NOTNUMBER

		return -1;
	}

	return 0;
}

static int count_argc(int argc, char **argv)
{
	int i = 0;

	for (; i < argc; i++) {
		if (!strcmp(argv[i], "--"))
			break;
	}

	return i;
}

int libxopt_parse(struct xopt *xopt, int argc, char **argv, void *data_base)
{
	char dummy0[] = "dummy", *argv_fixed[argc + 2], **work_argv = argv;
	int work_argc = argc;

	if (xopt->flags & LIBXOPT_FLAG_KEEPFIRST) {
		argv_fixed[0] = dummy0;
		for (int i = 0; i < argc + 1; i++) /* include NULL */
			argv_fixed[i + 1] = argv[i];

		work_argv = argv_fixed;
		work_argc++;
	}

	while (1) {
		xopt_option_callback_t callback = xopt_default_callback;
		const struct xopt_option *xoption = NULL;
		int opt, is_shortarg = 0;
		int ret;

		opt = getopt_long(count_argc(work_argc, work_argv),
				  work_argv, xopt->optstring,
				  xopt->longops, NULL);
		if (opt == -1)
			break;

		if (opt < XOPT_GETOPT_STARTVAL) {
			is_shortarg = 1;
			xoption = xopt_option_get_byname(xopt, (char)opt);
		} else {
			xoption = xopt_option_get_byval(xopt, opt);
		}

		if (!xoption) {
			libxopt_set_error(xopt, "xoption for opt %d(%c) "
						"is not found",
					  opt, isascii(opt) ? opt : ' ');
			return -1;
		}

		if (xoption->callback)
			callback = xoption->callback;

		ret = callback(xopt, xoption, is_shortarg, optarg, data_base);
		if (ret < 0)
			return ret;
	}

	if (xopt->flags & LIBXOPT_FLAG_KEEPFIRST) {
		for (int i = 0; i < argc + 1; i++)
			argv[i] = argv_fixed[i + 1];
		--optind;
	}

	return 0;
}

int libxopt_get_optind(struct xopt *xopt)
{
	return optind;
}
