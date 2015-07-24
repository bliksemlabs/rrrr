#include <check.h>
#include <stdlib.h>
#include <math.h>
#include "../util.h"
#include "../tdata_jit.h"

void dump_journey_patterns_at_stop (const char *filename, tdata_t *tdata);
void dump_journey_patterns_active  (const char *filename, tdata_t *tdata);

void dump_journey_patterns_at_stop (const char *filename, tdata_t *tdata) {
    uint32_t i;

    FILE *fp = fopen(filename, "w");
    #if 0
    i = tdata->n_journey_pattern_points;
    while (i) {
        i--;
        fprintf (fp, "%u\n", tdata->journey_patterns_at_stop[i]);
    }

    i = tdata->n_stop_points;
    while (i) {
        i--;
        fprintf (fp, "%u\n", tdata->stop_points[i].journey_patterns_at_stop_point_offset);
    }
    #endif

    for (i = 0; i < tdata->n_stop_points; ++i) {
        uint32_t j;
        fprintf (fp, "%u %u\n", i, tdata->stop_points[i].journey_patterns_at_stop_point_offset);

        for (j = tdata->stop_points[i].journey_patterns_at_stop_point_offset;
             j < tdata->stop_points[i+1].journey_patterns_at_stop_point_offset;
             j++) {
            fprintf (fp, "  %u\n", tdata->journey_patterns_at_stop[j]);
        }
    }

    fclose (fp);
}

void dump_journey_patterns_active (const char *filename, tdata_t *tdata) {
    uint32_t i;

    FILE *fp = fopen(filename, "w");
    for (i = 0; i < tdata->n_journey_patterns; ++i) {
        char bits[34] = "";
        renderBits(&tdata->journey_pattern_active[i], 4, bits);
        fprintf (fp, "%05u %s\n", i, bits);
    }
    fclose (fp);
}

START_TEST (test_tdata_jit)
    {
        tdata_t tdata;
        uint32_t *journey_patterns_active;
        jpidx_t *journey_patterns_at_stop;
        uint32_t n_journey_patterns_at_stop;
        uint32_t n_journey_patterns;

        memset (&tdata, 0, sizeof(tdata_t));

        ck_assert_msg (tdata_load (&tdata, "/tmp/timetable4.dat"), "Copy a working timetable4.dat file to /tmp/timetabel4.dat, and rerun the test.");
        journey_patterns_active = tdata.journey_pattern_active;
        journey_patterns_at_stop = tdata.journey_patterns_at_stop;
        n_journey_patterns_at_stop = tdata.n_journey_patterns_at_stop;

        dump_journey_patterns_at_stop ("/tmp/jp_sp_idx-timetable.txt", &tdata);
        dump_journey_patterns_active  ("/tmp/jp_active-timetable.txt", &tdata);

        ck_assert (tdata_journey_patterns_at_stop (&tdata));
        ck_assert_int_eq (n_journey_patterns_at_stop, tdata.n_journey_patterns_at_stop);
        dump_journey_patterns_at_stop ("/tmp/jp_sp_idx-jit.txt", &tdata);
        /* This one is not the same because the Python code doesn't sort the list */
        /* ck_assert (memcmp(journey_patterns_at_stop, tdata.journey_patterns_at_stop, tdata.n_journey_patterns_at_stop) == 0); */

        ck_assert (tdata_journey_patterns_index (&tdata));
        dump_journey_patterns_active ("/tmp/jp_active-jit.txt", &tdata);
        ck_assert (memcmp(journey_patterns_active, tdata.journey_pattern_active, tdata.n_journey_patterns) == 0);

        n_journey_patterns = tdata.n_journey_patterns;
        while (n_journey_patterns) {
            int delta;
            n_journey_patterns--;
            delta = tdata.journey_patterns[n_journey_patterns].max_time - tdata.journey_pattern_max[n_journey_patterns];
            if (delta != 0) {
                fprintf(stderr, "%d, %u %u %u %u %u\n", delta, n_journey_patterns, tdata.journey_pattern_min[n_journey_patterns], tdata.journey_pattern_max[n_journey_patterns], tdata.journey_patterns[n_journey_patterns].min_time, tdata.journey_patterns[n_journey_patterns].max_time);

            }
            ck_assert_int_eq (tdata.journey_pattern_min[n_journey_patterns], tdata.journey_patterns[n_journey_patterns].min_time);
            ck_assert_int_eq (tdata.journey_pattern_max[n_journey_patterns], tdata.journey_patterns[n_journey_patterns].max_time);
        }

    }
END_TEST

Suite *make_tdata_jit_suite(void);

Suite *make_tdata_jit_suite(void) {
    Suite *s = suite_create("tdata_jit");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_tdata_jit);
    suite_add_tcase(s, tc_core);
    return s;
}
