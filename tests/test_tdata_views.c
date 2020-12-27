#include <check.h>
#include <stdlib.h>
#include <strings.h>
#include "../util.h"
#include "../api.h"
#include "../rrrr_types.h"

START_TEST (test_tdata_views)
    {

        tdata_t tdata;
        rtime_t *result;
        uint32_t n_results;
        spidx_t sps[2];

        memset (&tdata, 0, sizeof(tdata_t));
        result = (rtime_t *) malloc (sizeof(rtime_t) * 256);
        sps[0] = 1;
        sps[1] = 15122;

        ck_assert_msg (tdata_load (&tdata, "/tmp/timetable4.dat"), "Copy a working timetable4.dat file to /tmp/timetabel4.dat, and rerun the test.");
        n_results = tdata_n_departures_since (&tdata, sps, 2, result, 256, 800 * 8, 800 * 9);
        while (n_results) {
            n_results--;
            printf("%u\n", result[n_results]);
        }

	tdata_close (&tdata);
	free (result);
    }
END_TEST

Suite *make_tdata_views_suite(void);

Suite *make_tdata_views_suite(void) {
    Suite *s = suite_create("tdata_views");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_tdata_views);
    suite_add_tcase(s, tc_core);
    return s;
}
