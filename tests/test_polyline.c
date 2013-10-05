#include <check.h>
#include "../polyline.h"

START_TEST (test_encode_one) {
    char buf[1024];
    int n = encode_double (-179.9832104, buf);
    ck_assert_int_eq (n, 6);
    ck_assert_str_eq ("`~oia@", buf);
} END_TEST

START_TEST (test_encode_latlon) {
    char buf[1024];
    latlon_t ll = {45.4545, 2.2222};
    int n = encode_latlon (ll, buf);
    //ck_assert_int_eq (n, ?);
    //ck_assert_str_eq ("?????????", buf);
} END_TEST

START_TEST (test_encode_polyline) {
    latlon_t lls[3] = { 
        { 38.500, -120.200},
        { 40.700, -120.950},
        { 43.252, -126.453} 
    };
    polyline_begin ();
    for (int i = 0; i < 3; ++i) polyline_point (lls[i]);
    // Different than result in the algortihm description 
    // because we are using floats not doubles.
    ck_assert_str_eq ("_p~iF|ps|U_ulLnnqC}lqNvxq`@", polyline_result ());    
    // Check that an empty polyline is zero-terminated.
    polyline_begin ();
    ck_assert (strlen(polyline_result()) == 0);
} END_TEST

/* Tests borrowed from https://code.google.com/p/py-gpolyencode/source/browse/trunk/tests/gpolyencode_tests.py */
START_TEST (test_glineenc) {

    latlon_t ll1[3] = {{38.5,-120.2}, {43.252,-126.453}, {40.7,-120.95}};
    polyline_begin ();
    for (int i = 0; i < 3; ++i) polyline_point (ll1[i]);
    ck_assert_str_eq ("_p~iF~ps|U_c_\\fhde@~lqNwxq`@", polyline_result());

    latlon_t ll2[3] = {{37.4419,-122.1419}, {37.4519,-122.1519}, {37.4619,-122.1819}};
    polyline_begin ();
    for (int i = 0; i < 3; ++i) polyline_point (ll2[i]);
    ck_assert_str_eq ("yzocFzynhVq}@n}@o}@nzD", polyline_result());
    
    latlon_t ll3 = {37.4419,-122.1419};
    polyline_begin ();
    polyline_point (ll3);
    ck_assert_str_eq ("_p~iF~ps|U", polyline_result());
    
    latlon_t ll4[5] = {{52.29834,8.94328}, {52.29767,8.93614}, {52.29322,8.93301}, {8.93036,52.28938}, {8.97475,52.27014}};
    polyline_begin ();
    for (int i = 0; i < 5; ++i) polyline_point (ll4[i]);
    ck_assert_str_eq ("soe~Hovqu@dCrk@xZpR~VpOfwBmtG", polyline_result());
    
} END_TEST

Suite *make_polyline_suite (void) {
    Suite *s = suite_create ("Polyline");
    TCase *tc_core = tcase_create ("Core");
    tcase_add_test  (tc_core, test_encode_one);
    tcase_add_test  (tc_core, test_encode_latlon);
    tcase_add_test  (tc_core, test_encode_polyline);
    tcase_add_test  (tc_core, test_glineenc);
    suite_add_tcase (s, tc_core);
    return s;
}

