#include <check.h>
#include <stdlib.h>
#include "../router_request.h"

START_TEST (test_from_epoch)
    {
        struct tm ltm;
        char date[11];
        router_request_t router_request;
        router_request_t *req = &router_request;

        tdata_t tdata;
        tdata_t *td = &tdata;
        td->utc_offset = 0;
        td->n_days = 2;

        /* Tue Mar 10 2015 01:00:00 GMT+0100 (CET) */
        td->calendar_start_time = 1425945600;

        router_request_from_epoch (req, td, 1425945600);
        ck_assert_int_eq (req->day_mask, 1);
        ck_assert_int_eq (router_request_to_date (req, td, &ltm), 1425945600);
        strftime(date, 11, "%Y-%m-%d", &ltm);
        ck_assert_str_eq (date, "2015-03-10");

        router_request_from_epoch (req, td, 1425945600 + 108000);
        ck_assert_int_eq (req->day_mask, 2);
        ck_assert_int_eq (router_request_to_date (req, td, &ltm), 1425945600 + 86400);
        router_request_to_date (req, td, &ltm);
        strftime(date, 11, "%Y-%m-%d", &ltm);
        ck_assert_str_eq (date, "2015-03-11");
    }
END_TEST

Suite *make_router_request_suite(void);

Suite *make_router_request_suite(void) {
    Suite *s = suite_create("router_request");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_from_epoch);
    suite_add_tcase(s, tc_core);
    return s;
}
