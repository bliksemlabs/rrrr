#include <check.h>
#include <stdlib.h>
#include "../util.h"

START_TEST (test_median_even)
    {
        float f[4] = {0.0f, 5.0f, 2.0f, 1.0f};
        float med, min, max;
        uint32_t n = 4;

        med = median(f, n, &min, &max);

        ck_assert(med == 1.5f);
        ck_assert_int_eq((int) min, 0);
        ck_assert_int_eq((int) max, 5);
    }
END_TEST

START_TEST (test_median_uneven)
    {
        float f[4] = {0.0f, 2.0f, 1.0f};
        float med, min, max;
        uint32_t n = 3;

        med = median(f, n, &min, &max);

        ck_assert(med == 1.0f);
        ck_assert_int_eq((int) min, 0);
        ck_assert_int_eq((int) max, 2);
    }
END_TEST

START_TEST (test_renderbits)
    {
        char out[34];
        uint32_t data;

        data = 14;
        renderBits(&data, 1, out);
        ck_assert_str_eq(out, "00001110");
    }
END_TEST

START_TEST (test_strtoepoch)
    {
        time_t out;
        out = strtoepoch("2013-12-11T10:09:08");
        ck_assert_int_eq(out, 1386752948);
    }
END_TEST

Suite *make_util_suite(void);

Suite *make_util_suite(void) {
    Suite *s = suite_create("util");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_median_even);
    tcase_add_test  (tc_core, test_median_uneven);
    tcase_add_test  (tc_core, test_renderbits);
    tcase_add_test  (tc_core, test_strtoepoch);
    suite_add_tcase(s, tc_core);
    return s;
}
