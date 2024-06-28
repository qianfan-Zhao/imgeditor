/*
 * api interface test
 * qianfan Zhao
 */

#ifndef API_TEST_H
#define API_TEST_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern size_t test_total, test_failed;

#define assert_streq(s1, s2) do {						\
	if (strcmp((s1), (s2))) {						\
		fprintf(stderr, "assert_streq failed in %s:%d (%s != %s)\n",	\
			__func__, __LINE__, s1, s2);				\
		test_failed++;							\
	}									\
	test_total++;								\
} while (0)

#define assert_inteq(a, b) do {							\
	if ((a) != (b)) {							\
		fprintf(stderr, "assert_eq failed in %s:%d (%d != %d)\n",	\
			__func__, __LINE__, a, b);				\
		test_failed++;							\
	}									\
	test_total++;								\
} while (0)

#define assert_good(ex) do {							\
	if (!(ex)) {								\
		fprintf(stderr, "assert(%s) failed in %s:%d\n", 		\
			#ex, __func__, __LINE__);				\
		test_failed++;							\
	}									\
	test_total++;								\
} while (0)

void xopt_test();
void hash_compatible_test();

#endif
