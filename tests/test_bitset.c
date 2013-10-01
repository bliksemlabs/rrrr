#include <check.h>
#include <stdlib.h>
#include "../bitset.h"

START_TEST (test_bitset) {
    uint32_t max = 50000;
    BitSet *bs = bitset_new(max);
    for (uint32_t i = 0; i < 50000; i += 2)
        bitset_set(bs, i);
    for (uint32_t i = 0; i < 10000; i++) {
        bitset_enumerate(bs);
    }
    bitset_destroy(bs);
} END_TEST

Suite *bitset_suite (void) {
    Suite *s = suite_create ("BitSet");
    TCase *tc_core = tcase_create ("Core");
    tcase_add_test  (tc_core, test_bitset);
    suite_add_tcase (s, tc_core);
    return s;
}

int main (void) {
    int number_failed;
    Suite *s = bitset_suite ();
    SRunner *sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
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


