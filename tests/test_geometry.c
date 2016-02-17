#include <check.h>
#include <stdlib.h>
#include <math.h>
#include "../geometry.h"

#define ck_assert_float_eq(a, b) ck_assert(fabs(a - b) < 0.00001f)

START_TEST (test_strtolatlon)
    {
        latlon_t latlon1;
        latlon_t latlon2;
        coord_t coord;
        double distance;

        strtolatlon("52.12000,4.50000", &latlon1);
        ck_assert_float_eq(latlon1.lat, 52.12000f);
        ck_assert_float_eq(latlon1.lon,  4.50000f);

        coord_from_latlon(&coord, &latlon1);
        ck_assert(coord.x == 32964396 &&
                  coord.y == 621815807);

        latlon_from_coord(&latlon2, &coord);
        distance = latlon_distance_meters(&latlon1, &latlon2);
        ck_assert(distance < 0.02797f);
    }
END_TEST

Suite *make_geometry_suite(void);

Suite *make_geometry_suite(void) {
    Suite *s = suite_create("geometry");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_strtolatlon);
    suite_add_tcase(s, tc_core);
    return s;
}
