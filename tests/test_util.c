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

START_TEST (test_epoch_to_rtime)
    {
        struct tm tm_out;
        rtime_t rtime;
        rtime_t rtime_expected = SEC_TO_RTIME(36548) + RTIME_ONE_DAY;

        rtime = epoch_to_rtime(1386752948, &tm_out);
        ck_assert_int_eq(rtime, rtime_expected);
        rtime = epoch_to_rtime(36548, &tm_out);
        ck_assert_int_eq(rtime, rtime_expected);
    }
END_TEST

START_TEST (test_rrrrandom)
    {
        uint32_t limit = 32;
        uint32_t a = rrrrandom(limit);
        uint32_t b = rrrrandom(limit);

        ck_assert_int_le(a, limit);
        ck_assert_int_le(b, limit);
        ck_assert_int_ne(a, b);
    }
END_TEST

START_TEST (test_btimetext)
    {
        char *out;
        char buf[13];
        rtime_t rt = SEC_TO_RTIME(36548);

        out = btimetext(UNREACHED, buf);
        ck_assert_str_eq(out, "   --   ");

        out = btimetext(rt, buf);
        ck_assert_str_eq(out, "10:09:08 -1D");

        out = btimetext(rt + RTIME_ONE_DAY, buf);
        ck_assert_str_eq(out, "10:09:08");

        out = btimetext(rt + RTIME_TWO_DAYS, buf);
        ck_assert_str_eq(out, "10:09:08 +1D");

        out = btimetext(RTIME_THREE_DAYS, buf);
        ck_assert_str_eq(out, "00:00:00 +2D");
    }
END_TEST

START_TEST (test_dedupRtime)
    {
        rtime_t i1[11] = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 6};
        rtime_t j1[5] = {1, 2, 3, 4, 6};
        rtime_t i2[1] = {8};
        rtime_t j2[1] = {8};

        uint32_t n;

        n = dedupRtime (i1, 11);

        ck_assert_int_eq (n, 5);
        ck_assert ( memcmp(i1, j1, 5) == 0 );

        n = dedupRtime (i2, 1);

        ck_assert_int_eq (n, 1);
        ck_assert ( memcmp(i2, j2, 1) == 0 );

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
    tcase_add_test  (tc_core, test_epoch_to_rtime);
    tcase_add_test  (tc_core, test_rrrrandom);
    tcase_add_test  (tc_core, test_btimetext);
    tcase_add_test  (tc_core, test_dedupRtime);
    suite_add_tcase(s, tc_core);
    return s;
}
