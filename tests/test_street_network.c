#include <check.h>
#include <stdlib.h>
#include "../street_network.h"
#include "../util.h"

START_TEST (test_mark_duration)
    {
        street_network_t street_network;
        street_network_t *sn = &street_network;
        bool out;

        street_network_init (sn);

        sn->stop_points[0] = 1;
        sn->stop_points[1] = 3;
        sn->n_points = 2;

        out = street_network_mark_duration_to_stop_point (sn, 3, 200);
        ck_assert (out == true);

        out = street_network_mark_duration_to_stop_point (sn, 2, 50);
        ck_assert (out == true);

        out = street_network_mark_duration_to_stop_point (sn, 1, 100);
        ck_assert (out == true);

        ck_assert_int_eq (sn->durations[0], 100);
        ck_assert_int_eq (sn->durations[1], 200);
        ck_assert_int_eq (sn->durations[2],  50);

        ck_assert_int_eq (sn->stop_points[0], 1);
        ck_assert_int_eq (sn->stop_points[1], 3);
        ck_assert_int_eq (sn->stop_points[2], 2);

        sn->n_points = RRRR_MAX_ENTRY_EXIT_POINTS;
        rrrr_memset(sn->stop_points, 0, RRRR_MAX_ENTRY_EXIT_POINTS);
        out = street_network_mark_duration_to_stop_point (sn, 4, 100);
        ck_assert (out == false);
    }
END_TEST

Suite *make_street_network_suite(void);

Suite *make_street_network_suite(void) {
    Suite *s = suite_create("street_network");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_mark_duration);
    suite_add_tcase(s, tc_core);
    return s;
}
