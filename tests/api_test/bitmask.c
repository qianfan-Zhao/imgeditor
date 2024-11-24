#include "api_test.h"
#include "bitmask.h"

void bitmask_test(void)
{
	struct bitmask *b = alloc_bitmask(511);
	int continue_count = 0;
	int count_one = 0;

	assert_inteq((int)b->bufsize, 512 / 8);

	bitmask_set(b, 1);
	bitmask_set(b, 3);
	bitmask_set(b, 5);
	bitmask_set_bits(b, 31, 2);
	bitmask_set(b, 500);

	bitmask_foreach(i, b) {
		assert_inteq(bitmask_get(b, i), 1);
		count_one++;
	}
	assert_inteq(count_one, 6);

	/* the buffer should not overflow */
	bitmask_set(b, 100000000);

	assert_inteq(bitmask_get(b, 1), 1);
	assert_inteq(bitmask_get(b, 3), 1);
	assert_inteq(bitmask_get(b, 5), 1);
	assert_inteq(bitmask_get(b, 31), 1);
	assert_inteq(bitmask_get(b, 32), 1);
	assert_inteq(bitmask_get(b, 500), 1);

	for (int i = 0; i < 511; i++) {
		switch (i) {
		case 1:
		case 3:
		case 5:
		case 31:
		case 32:
		case 500:
			break;
		default:
			assert_inteq(bitmask_get(b, i), 0);
			break;
		}
	}

	/* bit index  0  1  2  3  4  5  6 ... 30 31 32 33 ... 500
	 * bit value  0  1  0  1  0  1  0 ...  0  1  1  0 ... 1
	 */
	assert_inteq(bitmask_next_zero(b, 0), 0);
	assert_inteq(bitmask_next_zero(b, 1), 2);
	assert_inteq(bitmask_next_zero(b, 3), 4);
	assert_inteq(bitmask_next_zero(b, 31), 33);

	assert_inteq(bitmask_next_one(b, 0), 1);
	assert_inteq(bitmask_next_one(b, 2), 3);
	assert_inteq(bitmask_next_one(b, 6), 31);
	assert_inteq(bitmask_next_one(b, 32), 32);
	assert_inteq(bitmask_next_one(b, 33), 500);
	assert_inteq(bitmask_next_one(b, 501), -1);

	bitmask_foreach_continue(it, b) {
		switch (continue_count) {
		case 0:
			assert_inteq(it.start, 1);
			assert_inteq(it.bits,  1);
			break;
		case 1:
			assert_inteq(it.start, 3);
			assert_inteq(it.bits,  1);
			break;
		case 2:
			assert_inteq(it.start, 5);
			assert_inteq(it.bits,  1);
			break;
		case 3:
			assert_inteq(it.start, 31);
			assert_inteq(it.bits,   2);
			break;
		case 4:
			assert_inteq(it.start, 500);
			assert_inteq(it.bits,  1);
			break;
		}

		continue_count++;
	}
	assert_inteq(continue_count, 5);

	bitmask_unset(b, 32);
	assert_inteq(bitmask_get(b, 32), 0);
}
