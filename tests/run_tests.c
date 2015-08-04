#include <check.h>
#include <stdlib.h>

/* could be in a header, but simpler here */
Suite *make_bitset_suite (void);
Suite *make_geometry_suite (void);
Suite *make_hashgrid_suite (void);
Suite *make_json_suite (void);
Suite *make_polyline_suite (void);
Suite *make_radixtree_suite (void);
Suite *make_router_request_suite (void);
Suite *make_street_network_suite (void);
#ifdef RRRR_TDATA_IO_DYNAMIC
Suite *make_tdata_jit_suite (void);
#endif
Suite *make_tdata_views_suite (void);
Suite *make_util_suite (void);

static Suite *make_master_suite (void) {
    Suite *s = suite_create ("Master");
    return s;
}

int main (void) {
    int number_failed;
    SRunner *sr;
    sr = srunner_create (make_master_suite ());
    srunner_add_suite (sr, make_bitset_suite ());
    srunner_add_suite (sr, make_geometry_suite ());
    srunner_add_suite (sr, make_json_suite ());
    srunner_add_suite (sr, make_hashgrid_suite ());
    srunner_add_suite (sr, make_polyline_suite ());
    srunner_add_suite (sr, make_radixtree_suite ());
    srunner_add_suite (sr, make_router_request_suite ());
    srunner_add_suite (sr, make_street_network_suite ());
    srunner_add_suite (sr, make_util_suite ());
    srunner_add_suite (sr, make_tdata_views_suite ());
#ifdef RRRR_TDATA_IO_DYNAMIC
    srunner_add_suite (sr, make_tdata_jit_suite ());
#endif
    srunner_set_log (sr, "test.log");
    srunner_run_all (sr, CK_VERBOSE); /* CK_NORMAL */
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

