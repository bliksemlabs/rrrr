#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "../bitset.h"
#include "../hashgrid.h"
#include "../tdata.h"

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

Suite *make_bitset_suite (void) {
    Suite *s = suite_create ("BitSet");
    TCase *tc_core = tcase_create ("Core");
    tcase_add_test  (tc_core, test_bitset);
    suite_add_tcase (s, tc_core);
    return s;
}

static void geometry_test (latlon_t *lls, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        latlon_dump (lls + i);
        coord_t coord;
        coord_from_latlon(&coord, lls + i);
        coord.x = -coord.x;
        latlon_t ll;
        latlon_from_coord (&ll, &coord);
        latlon_dump (&ll);        
        coord_dump (&coord);
        coord_from_latlon(&coord, &ll);
        coord_dump (&coord);
    }
}

START_TEST (test_hashgrid) {
    tdata_t tdata;
    tdata_load ("timetable.dat", &tdata); // TODO replace with independent test data
    // geometry_test (tdata.stop_coords, tdata.n_stops);
    HashGrid hg;
    coord_t coords[tdata.n_stops];
    for (uint32_t c = 0; c < tdata.n_stops; ++c) {
        coord_from_latlon(coords + c, tdata.stop_coords + c);
    }
    HashGrid_init (&hg, 100, 500.0, coords, tdata.n_stops);
    // HashGrid_dump (&hg);
    coord_t qc;
    coord_from_lat_lon (&qc, 52.37790, 4.89787);
    coord_dump (&qc);
    double radius_meters = 150;
    HashGridResult result;
    HashGrid_query (&hg, &result, qc, radius_meters);
    uint32_t item;
    double distance;
    while ((item = HashGridResult_next_filtered(&result, &distance)) != HASHGRID_NONE) {
        latlon_t *ll = tdata.stop_coords + item;
        // latlon_dump (tdata.stop_coords + item);
        printf ("%d,%f,%f,%f\n", item, ll->lat, ll->lon, distance);
    }
    HashGrid_teardown (&hg);
} END_TEST

Suite *make_hashgrid_suite (void) {
    Suite *s = suite_create ("HashGrid");
    TCase *tc_core = tcase_create ("Core");
    tcase_add_test  (tc_core, test_hashgrid);
    suite_add_tcase (s, tc_core);
    return s;
}

Suite *make_master_suite (void) {
    Suite *s = suite_create ("Master");
    return s;
}

int main (void) {
    int number_failed;
    SRunner *sr;
    sr = srunner_create (make_master_suite ());
    srunner_add_suite (sr, make_bitset_suite ());
    srunner_add_suite (sr, make_hashgrid_suite ());
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


