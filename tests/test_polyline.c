#include <check.h>
#include <stdlib.h>
#include "../polyline.h"

START_TEST (test_polyline)
    {
        char *out;
        uint32_t n;
        polyline_t pl;

        /* https://developers.google.com/maps/documentation/utilities/polylinealgorithm */
        polyline_begin(&pl);
        polyline_point(&pl, 38.5f, -120.2f);
        polyline_point(&pl, 40.7f, -120.95f);
        polyline_point(&pl, 43.252, -126.453f);

        n = polyline_length(&pl);
        ck_assert_int_eq(n, 3);

        out = polyline_result(&pl);
        ck_assert_str_eq(out, "_p~iF~ps|U_ulLnnqC_mqNvxq`@");
    }
END_TEST

Suite *make_polyline_suite(void);

Suite *make_polyline_suite(void) {
    Suite *s = suite_create("polyline");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_polyline);
    suite_add_tcase(s, tc_core);
    return s;
}
