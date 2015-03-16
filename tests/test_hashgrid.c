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

Suite *make_hashgrid_suite(void);

Suite *make_hashgrid_suite(void) {
    Suite *s = suite_create("hashgrid");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_hashgrid);
    suite_add_tcase(s, tc_core);
    return s;
}
