/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

//#include "reversal.h"
#include "router.h"
#include "tdata.h"
#include "list.h"
#include <stdbool.h>
#include <stdio.h>

#define OUTPUT_LEN 8000

void reversal (tdata_t *tdata, router_request_t *req, bool verbose) {

    /* Initialize router */
    router_t router;
    router_setup (&router, tdata);

    char result_buf[OUTPUT_LEN];
    router_route (&router, req);
    if (verbose) {
        router_request_dump (&router, req);
        router_result_dump(&router, req, result_buf, OUTPUT_LEN);
        printf("%s", result_buf);
    }

    /* Repeat search in reverse to compress transfers */
    uint32_t n_reversals = req->arrive_by ? 1 : 2;
    /* but do not reverse requests starting on board (they cannot be compressed, earliest arrival is good enough) */
    if (req->start_trip_trip != NONE) n_reversals = 0;

    for (uint32_t i = 0; i < n_reversals; ++i) {
        router_request_reverse (&router, req); // handle case where route is not reversed
        router_route (&router, req);
        if (verbose) {
            printf ("Repeated search with reversed request: \n");
            router_request_dump (&router, req);
            router_result_dump(&router, req, result_buf, OUTPUT_LEN);
            printf("%s", result_buf);
        }
    }

    /* Output only final result in non-verbose mode */
    if (!verbose) {
        router_result_dump(&router, req, result_buf, OUTPUT_LEN);
        printf("%s", result_buf);
    }

    router_teardown(&router);

}
