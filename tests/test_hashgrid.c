#include <check.h>
#include <stdlib.h>
#include "../hashgrid.h"
#include "../geometry.h"

START_TEST (test_hashgrid)
    {
#if 0
        double distance;
#endif
        hashgrid_result_t result;
        coord_t qc;
        uint32_t item;

        coord_t coords[7] = { {    0,   0 },
                              {    0,  10 },
                              {   10,   0 },
                              {   10,  10 },
                              {    0, -10 },
                              {  -10,   0 },
                              {  -10, -10 } };


        hashgrid_t hashgrid;
        hashgrid_t *hg = &hashgrid;

        ck_assert (hashgrid_init(hg, 2, METERS_PER_BRAD, coords, 7));

        hashgrid_dump (hg);

        qc.x = 10;
        qc.y = 10;

        hashgrid_query (hg, &result, qc, 4 * METERS_PER_BRAD);
        item = hashgrid_result_closest (&result);
        ck_assert_int_eq (item, 3);

#if 0
        /* TODO: This part should be tested */
        hashgrid_query (hg, &result, qc, 4 * METERS_PER_BRAD);
        item = hashgrid_result_next(&result);
        ck_assert_int_eq (item, 3);

        item = hashgrid_result_next_filtered(&result, &distance);
        ck_assert_int_eq (item, HASHGRID_NONE);
#endif

        hashgrid_teardown (hg);
    }
END_TEST

START_TEST (test_hashgrid_init)
    {
        coord_t coords[2];
        double distance;

        hashgrid_t hg;
        hashgrid_result_t result;

        coord_from_lat_lon(&coords[0], 1.0, 1.0);
        coord_from_lat_lon(&coords[1], 2.0, 2.0);

        hashgrid_init(&hg, 100, 500, coords, 2);
        hashgrid_query (&hg, &result, coords[1], 500.0);

        distance = 0.0f;
        while(hashgrid_result_next_filtered(&result, &distance) != HASHGRID_NONE) {

        }

        hashgrid_teardown (&hg);
    }
END_TEST

Suite *make_hashgrid_suite(void);

Suite *make_hashgrid_suite(void) {
    Suite *s = suite_create("hashgrid");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_hashgrid);
    tcase_add_loop_test  (tc_core, test_hashgrid_init, 0, 20);
    suite_add_tcase(s, tc_core);
    return s;
}
