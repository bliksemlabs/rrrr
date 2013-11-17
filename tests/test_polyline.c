#include <check.h>
#include "../polyline.h"

/* Allow testing at full double resolution instead of floats in latlon_t */
struct latlon_double {
    double lat;
    double lon;
};

static void encode (struct latlon_double *lls, int n) {
    polyline_begin ();
    for (int i = 0; i < n; ++i) {
        polyline_point (lls[i].lat, lls[i].lon);
    }
}

static bool str_endswith (char *str, char *end) {
    if (str == NULL || end == NULL) return false;
    size_t str_size = strlen(str);
    size_t end_size = strlen(end);
    if (str_size < end_size) return false;
    char *s = str + str_size;
    char *e = end + end_size;
    for (size_t i = 0; i < end_size; ++i) {
        if (*(e--) != *(s--)) return false;
    }
    return true;
}

static bool str_startswith (char *str, char *pre) {
    if (str == NULL || pre == NULL) return false;
    size_t str_size = strlen(str);
    size_t pre_size = strlen(pre);
    if (str_size < pre_size) return false;
    for (size_t i = 0; i < pre_size; ++i) {
        if (str[i] != pre[i]) return false;
    }
    return true;
}

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

START_TEST (test_encode_zeros) {
    struct latlon_double lls[4] = {{0, 0}, { 40.700, -120.950}, { 40.700, -120.950}, { 40.700, -120.950}};
    encode (lls, 4);
    ck_assert (str_endswith(polyline_result(), "????"));
    ck_assert (str_startswith(polyline_result(), "??"));
} END_TEST

START_TEST (test_encode_polyline) {
    struct latlon_double lls[3] = { 
        { 38.500, -120.200},
        { 40.700, -120.950},
        { 43.252, -126.453} 
    };
    encode (lls, 3);
    ck_assert_str_eq ("_p~iF|ps|U_ulLnnqC}lqNvxq`@", polyline_result ());    
    // Check that an empty polyline is zero-terminated.
    polyline_begin ();
    ck_assert (strlen(polyline_result()) == 0);
} END_TEST

/* Tests borrowed from https://code.google.com/p/py-gpolyencode/source/browse/trunk/tests/gpolyencode_tests.py */
/* Failing due to a few individual characters being off by 2. Lowest order bit is sign bit, so this is probably a rounding error. */
START_TEST (test_glineenc) {

    struct latlon_double llA[3] = {{38.5,-120.2}, {43.252,-126.453}, {40.7,-120.95}};
    encode (llA, 3);
    ck_assert_str_eq ("_p~iF~ps|U_c_\\fhde@~lqNwxq`@", polyline_result()); // note escaped backslash

    struct latlon_double llB[3] = {{37.4419,-122.1419}, {37.4519,-122.1519}, {37.4619,-122.1819}};
    encode (llB, 3);
    ck_assert_str_eq ("yzocFzynhVq}@n}@o}@nzD", polyline_result());
    
    struct latlon_double llC[1] = {{37.4419,-122.1419}};
    encode (llC, 1);
    ck_assert_str_eq ("_p~iF~ps|U", polyline_result());
    
    struct latlon_double llD[5] = {{52.29834,8.94328}, {52.29767,8.93614}, {52.29322,8.93301}, {8.93036,52.28938}, {8.97475,52.27014}};
    encode (llD, 5);
    ck_assert_str_eq ("soe~Hovqu@dCrk@xZpR~VpOfwBmtG", polyline_result());
    
} END_TEST

Suite *make_polyline_suite (void) {
    Suite *s = suite_create ("Polyline");
    TCase *tc_core = tcase_create ("Core");
    tcase_add_test  (tc_core, test_encode_zeros);
    tcase_add_test  (tc_core, test_encode_one);
    tcase_add_test  (tc_core, test_encode_latlon);
    tcase_add_test  (tc_core, test_encode_polyline);
    tcase_add_test  (tc_core, test_glineenc);
    suite_add_tcase (s, tc_core);
    return s;
}

