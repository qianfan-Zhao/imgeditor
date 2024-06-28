#include <stdio.h>
#include <stdlib.h>
#include "api_test.h"

size_t test_total = 0, test_failed = 0;

int main(int argc, char **argv)
{
	xopt_test();
	hash_compatible_test();

	printf("total %zu, failed %zu\n", test_total, test_failed);
	if (test_failed)
		exit(EXIT_FAILURE);

	return 0;
}
