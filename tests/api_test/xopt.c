#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "string_helper.h"
#include "api_test.h"
#include "libxopt.h"

struct config {
	int		ia, ib, ic;
	long		la, lb, lc;
	unsigned long	ula, ulb, ulc;
	bool		m;
	bool		n;
	bool		help;

	const char	*s;
};

static const struct xopt_option options[] = {
	{
		.long_name	= "ia",
		.name		= 'a',
		.offset		= offsetof(struct config, ia),
		.type		= XOPT_TYPE_INT,
	}, {
		.long_name	= "ib",
		.name		= 'b',
		.offset		= offsetof(struct config, ib),
		.type		= XOPT_TYPE_INT_DEC,
	}, {
		.long_name	= "ic",
		.name		= 'c',
		.offset		= offsetof(struct config, ic),
		.type		= XOPT_TYPE_INT_HEX,
	}, {
		.long_name	= "la",
		.offset		= offsetof(struct config, la),
		.type		= XOPT_TYPE_LONG,
	}, {
		.long_name	= "lb",
		.offset		= offsetof(struct config, lb),
		.type		= XOPT_TYPE_LONG_DEC,
	}, {
		.long_name	= "lc",
		.offset		= offsetof(struct config, lc),
		.type		= XOPT_TYPE_LONG_HEX,
	}, {
		.long_name	= "ula",
		.offset		= offsetof(struct config, ula),
		.type		= XOPT_TYPE_ULONG,
	}, {
		.long_name	= "ulb",
		.offset		= offsetof(struct config, ulb),
		.type		= XOPT_TYPE_ULONG_DEC,
	}, {
		.long_name	= "ulc",
		.offset		= offsetof(struct config, ulc),
		.type		= XOPT_TYPE_ULONG_HEX,
	}, {
		.name		= 'm',
		.offset		= offsetof(struct config, m),
		.type		= XOPT_TYPE_BOOL,
	}, {
		.name		= 'n',
		.offset		= offsetof(struct config, n),
		.type		= XOPT_TYPE_BOOL,
	}, {
		.long_name	= "help",
		.name		= 'h',
		.offset		= offsetof(struct config, help),
		.type		= XOPT_TYPE_BOOL,
	}, {
		.long_name	= "string",
		.name		= 's',
		.offset		= offsetof(struct config, s),
		.type		= XOPT_TYPE_STRING,
	},
	LIBXOPT_NULLOPTION
};

#define MAXARGV 128
static char xopt_buffer[1024], *xopt_argv[MAXARGV];
static int xopt_argc;

static void xopt_buffer_setup(const char *command)
{
	snprintf(xopt_buffer, sizeof(xopt_buffer), "%s", command);
	xopt_argc = argv_buffer_split_with_delim(xopt_buffer, " ",
						 xopt_argv, MAXARGV);
}

static int xopt_run(struct xopt *xopt, const char *command, struct config *c)
{
	xopt_buffer_setup(command);

	memset(c, 0, sizeof(*c));
	libxopt_reinit(xopt);

	return libxopt_parse(xopt, xopt_argc, xopt_argv, c);
}

static void xopt_test_start0(void)
{
	struct xopt *xopt = libxopt_new(options, LIBXOPT_FLAG_KEEPFIRST);
	struct config c = { 0 };
	int optind, ret;

	if (!xopt)
		return;

	ret = xopt_run(xopt, "-h", &c);
	assert_inteq(ret, 0);
	assert_good(c.help);

	/* string */
	xopt_run(xopt, "-s hello", &c);
	assert_good(c.s);
	if (c.s)
		assert_streq(c.s, "hello");

	/* auto base mode */
	xopt_run(xopt, " -a 4", &c);
	assert_good(c.ia == 4);
	xopt_run(xopt, " -a 0x08", &c);
	assert_good(c.ia == 8);

	/* negative number */
	xopt_run(xopt, " --ia -4 --la -8", &c);
	assert_good(c.ia == -4);
	assert_good(c.la == -8);

	/* glue mode */
	xopt_run(xopt, " --ia 4 --la 8 -mnh", &c);
	assert_good(c.ia == 4);
	assert_good(c.la == 8);
	assert_good(c.m);
	assert_good(c.n);
	assert_good(c.help);

	xopt_run(xopt, " -a 4 -b 5 -c 0x06 --help --string abc", &c);
	assert_good(c.ia == 4);
	assert_good(c.ib == 5);
	assert_good(c.ic == 6);
	assert_good(c.help);
	assert_good(c.s);
	if (c.s)
		assert_streq(c.s, "abc");

	xopt_run(xopt, " --ia 8 -b 6 -c 0x07", &c);
	assert_good(c.ia == 8);
	assert_good(c.ib == 6);
	assert_good(c.ic == 7);
	assert_good(!c.help);

	ret = xopt_run(xopt, " -c 0xffr", &c);
	assert_good(ret < 0);

	/* with extra arg and it will be sorted as:
	 *   0  1   2  3   4  5  6    7    8
	 * --ia 1 --ib 2 --ic 3 ext1 ext2 ext3
	 */
	ret = xopt_run(xopt, " ext1 ext2 ext3 --ia 1 --ib 2 --ic 3", &c);
	assert_good(ret == 0);

	optind = libxopt_get_optind(xopt);
	assert_inteq(optind, 6);
	assert_streq(xopt_argv[optind + 0], "ext1");
	assert_streq(xopt_argv[optind + 1], "ext2");
	assert_streq(xopt_argv[optind + 2], "ext3");

	/* with extra arg and it will be sorted as:
	 *   0  1   2  3   4  5  6    7    8   9  10     11  12
	 * --ia 1 --ib 2 --ic 3 ext1 ext2 ext3 -- ext4 --ia ext5
	 */
	ret = xopt_run(xopt, " ext1 ext2 ext3 --ia 1 --ib 2 --ic 3 -- ext4 --ia ext5", &c);
	assert_good(ret == 0);

	optind = libxopt_get_optind(xopt);
	assert_inteq(optind, 6);
	assert_streq(xopt_argv[optind + 0], "ext1");
	assert_streq(xopt_argv[optind + 1], "ext2");
	assert_streq(xopt_argv[optind + 2], "ext3");
	assert_streq(xopt_argv[optind + 3], "--");
	assert_streq(xopt_argv[optind + 4], "ext4");
	assert_streq(xopt_argv[optind + 5], "--ia");
	assert_streq(xopt_argv[optind + 6], "ext5");

	libxopt_free(xopt);
}

static void xopt_test_start1(void)
{
	struct xopt *xopt = libxopt_new(options, 0);
	struct config c = { 0 };
	int optind, ret;

	if (!xopt)
		return;

	ret = xopt_run(xopt, "xopt -h", &c);
	assert_inteq(ret, 0);
	assert_good(c.help);

	/* auto base mode */
	xopt_run(xopt, "xopt -a 4", &c);
	assert_good(c.ia == 4);
	xopt_run(xopt, "xopt -a 0x08", &c);
	assert_good(c.ia == 8);

	/* negative number */
	xopt_run(xopt, "xopt --ia -4 --la -8", &c);
	assert_good(c.ia == -4);
	assert_good(c.la == -8);

	/* glue mode */
	xopt_run(xopt, "xopt --ia 4 --la 8 -mnh", &c);
	assert_good(c.ia == 4);
	assert_good(c.la == 8);
	assert_good(c.m);
	assert_good(c.n);
	assert_good(c.help);

	xopt_run(xopt, "xopt -a 4 -b 5 -c 0x06 --help --string string", &c);
	assert_good(c.ia == 4);
	assert_good(c.ib == 5);
	assert_good(c.ic == 6);
	assert_good(c.help);
	assert_good(c.s);
	if (c.s)
		assert_streq(c.s, "string");

	xopt_run(xopt, "xopt --ia 8 -b 6 -c 0x07", &c);
	assert_good(c.ia == 8);
	assert_good(c.ib == 6);
	assert_good(c.ic == 7);
	assert_good(!c.help);

	ret = xopt_run(xopt, "xopt -c 0xffr", &c);
	assert_good(ret < 0);

	/* with extra arg and it will be sorted as:
	 * 0      1  2   3  4   5  6  7    8    9
	 * xopt --ia 1 --ib 2 --ic 3 ext1 ext2 ext3
	 */
	ret = xopt_run(xopt, "xopt ext1 ext2 ext3 --ia 1 --ib 2 --ic 3", &c);
	assert_good(ret == 0);

	optind = libxopt_get_optind(xopt);
	assert_inteq(optind, 7);
	assert_streq(xopt_argv[optind + 0], "ext1");
	assert_streq(xopt_argv[optind + 1], "ext2");
	assert_streq(xopt_argv[optind + 2], "ext3");

	/* with extra arg and it will be sorted as:
	 *   0    1  2   3  4   5  6  7    8   9    10  11   12   13
	 * xopt --ia 1 --ib 2 --ic 3 ext1 ext2 ext3 -- ext4 --ia ext5
	 */
	ret = xopt_run(xopt, "xopt ext1 ext2 ext3 --ia 1 --ib 2 --ic 3 -- ext4 --ia ext5", &c);
	assert_good(ret == 0);

	optind = libxopt_get_optind(xopt);
	assert_inteq(optind, 7);
	assert_streq(xopt_argv[optind + 0], "ext1");
	assert_streq(xopt_argv[optind + 1], "ext2");
	assert_streq(xopt_argv[optind + 2], "ext3");
	assert_streq(xopt_argv[optind + 3], "--");
	assert_streq(xopt_argv[optind + 4], "ext4");
	assert_streq(xopt_argv[optind + 5], "--ia");
	assert_streq(xopt_argv[optind + 6], "ext5");

	libxopt_free(xopt);
}

void xopt_test(void)
{
	xopt_test_start1();

	xopt_test_start0();
}
