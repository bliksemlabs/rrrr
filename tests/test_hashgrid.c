#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "../hashgrid.h"
#include "../tdata.h"

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
        // printf ("%d,%f,%f,%f\n", item, ll->lat, ll->lon, distance);
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

