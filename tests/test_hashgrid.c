#include <check.h>
#include <stdlib.h>
#include "../hashgrid.h"
#include "../geometry.h"

START_TEST (test_hashgrid)
    {
        double distance;
        hashgrid_result_t result;
        coord_t qc;
        uint32_t item;

        coord_t coords[7] = { {  0,  0 },
                              {  0,  1 },
                              {  1,  0 },
                              {  1,  1 },
                              {  0, -1 },
                              { -1,  0 },
                              { -1, -1 } };


        hashgrid_t hashgrid;
        hashgrid_t *hg = &hashgrid;

        ck_assert (hashgrid_init(hg, 2, METERS_PER_BRAD, coords, 7));

        hashgrid_dump (hg);

#if 0
        qc.x = 0;
        qc.y = 1;

        hashgrid_query (hg, &result, qc, 4 * METERS_PER_BRAD);

        item = hashgrid_result_next(&result);
        ck_assert_int_eq (item, 1);

        item = hashgrid_result_next_filtered(&result, &distance);
        ck_assert_int_eq (item, HASHGRID_NONE);
#endif

        hashgrid_teardown (hg);
    }
END_TEST

START_TEST (test_hashgrid_init)
    {
        coord_t coords[2];
        coord_from_lat_lon(&coords[0], 1.0, 1.0);
        coord_from_lat_lon(&coords[1], 2.0, 2.0);

        hashgrid_t hg;
        hashgrid_result_t result;

        hashgrid_init(&hg, 100, 500, coords, 2);
        hashgrid_query (&hg, &result, coords[1], 500.0);

        double distance = 0.0;
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
