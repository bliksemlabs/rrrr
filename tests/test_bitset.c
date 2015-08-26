#include <check.h>
#include <stdlib.h>
#include "../bitset.h"

START_TEST (test_bitset)
    {
        uint32_t i;
        uint32_t max = 50000;
        bitset_t *bs = bitset_new(max);
        bitset_t *bs_inv = bitset_new(max);

        for (i = 0; i < 50000; i += 2)
            bitset_set(bs, i);
        
        ck_assert_int_eq (bitset_count(bs), 25000);
        
        for (i = 1; i < 50000; i += 2)
            bitset_set(bs_inv, i);

        ck_assert_int_eq (bitset_count(bs_inv), 25000);
        
        for (i = 0; i < 50000; ++i){
            if (i % 2 == 0){
                ck_assert(bitset_get(bs, i));
                ck_assert(!bitset_get(bs_inv, i));
            }else{
                ck_assert(bitset_get(bs_inv, i));
                ck_assert(!bitset_get(bs, i));
            }
        }
        ck_assert(bitset_get(bs, 0));
        ck_assert(!bitset_get(bs, 1));
        ck_assert_int_eq(2, bitset_next_set_bit(bs, 1));
        ck_assert_int_eq(0, bitset_next_set_bit(bs, 0));
        ck_assert_int_eq(BITSET_NONE, bitset_next_set_bit(bs, 50000));
        ck_assert_int_eq(BITSET_NONE, bitset_next_set_bit(bs, 50001));

        ck_assert(!bitset_get(bs_inv, 0));
        ck_assert(bitset_get(bs_inv, 1));
        ck_assert_int_eq(1, bitset_next_set_bit(bs_inv, 1));
        ck_assert_int_eq(3, bitset_next_set_bit(bs_inv, 2));
        ck_assert_int_eq(BITSET_NONE, bitset_next_set_bit(bs_inv, 50000));
        ck_assert_int_eq(BITSET_NONE, bitset_next_set_bit(bs_inv, 50001));

        bitset_mask_and(bs, bs_inv);
        for (i = 0; i < 50000; ++i) {
            ck_assert(!bitset_get(bs, i));
        }

        /* Test flipping all bits on */
        bitset_black(bs);
        ck_assert_int_eq(bitset_count(bs), 50000);
        for (i = 0; i < 50000; ++i) {
            ck_assert(bitset_get(bs, i));
        }

        /* Test clearing all bits */
        bitset_clear(bs);
        ck_assert_int_eq(bitset_count(bs), 0);
        ck_assert_int_eq(BITSET_NONE,bitset_next_set_bit(bs, 0));

        /* Test counting */
        bitset_set(bs, 49999);
        bitset_set(bs, 39999);
        bitset_set(bs, 29999);
        bitset_set(bs, 19999);
        bitset_set(bs,  1);
        bitset_set(bs,  0);
        ck_assert_int_eq (bitset_count(bs), 5);

        /* Test unset */
        bitset_black(bs);
        for (i = 0; i < 50000; i += 2) {
            bitset_unset(bs, i);
        }

        ck_assert_int_eq(bitset_count(bs), 25000);
        ck_assert(!bitset_get(bs, 0));
        ck_assert(bitset_get(bs, 1));
        ck_assert_int_eq(1, bitset_next_set_bit(bs, 1));
        ck_assert_int_eq(3, bitset_next_set_bit(bs, 2));
        ck_assert_int_eq(BITSET_NONE, bitset_next_set_bit(bs, 50000));
        ck_assert_int_eq(BITSET_NONE, bitset_next_set_bit(bs, 50001));

        bitset_destroy(bs);
        bitset_destroy(bs_inv);
    }
END_TEST

Suite *make_bitset_suite(void);

Suite *make_bitset_suite(void) {
    Suite *s = suite_create("bitset_t");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_bitset);
    suite_add_tcase(s, tc_core);
    return s;
}

/*

Enumerating very sparse 50kbit bitset 100k times:
10.592 sec without skipping chunks == 0
0.210 sec with skipping

Enumerating very dense 50kbit bitset 100k times:
20.525 sec without skipping
20.599 sec with skipping

So no real slowdown from skipping checks, but great improvement on sparse sets.
Maybe switching to 32bit ints would allow finer-grained skipping.

Why is the dense set so much slower? Probably function call overhead (1 per result).
For dense set:
Adding inline keyword and -O2 reduces to 6.1 seconds.
Adding inline keyword and -O3 reduces to 7.6 seconds (note this is higher than -O2).
Adding -O2 without inline keyword reduces to 9.8 seconds.
Adding -O3 without inline keyword reduces to 7.6 seconds.

So enumerating the dense set 8 times takes roughly 0.0005 sec.

For sparse set:
Adding inline keyword and -O2 reduces to 0.064 seconds (0.641 sec for 1M enumerations).

*/


