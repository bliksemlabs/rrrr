/* tester.c : single-theaded test of router for unit tests and debugging */

#define _GNU_SOURCE

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "rrrr.h"
#include "tdata.h"
#include "router.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage:\n%s << from >> << to >> [ (A)rrive | (D)epart ] [ iso-timestamp ] [ timetable.dat ]\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    router_request_t req;
    router_request_initialize (&req);
    req.from = strtol(argv[1], NULL, 10);
    req.to = strtol(argv[2], NULL, 10);

    //srand(time(NULL));
    //router_request_randomize(&req);
    
    if (req.from == req.to) {  
        fprintf(stderr, "Dude, you are already there.\n");
        exit(-1);
    }

    if (argc >= 4) {
        if (argv[3][0] == 'A' || argv[3][0] == 'a') {
            req.arrive_by = true;
        } else if (argv[3][0] == 'D' || argv[3][0] == 'd') {
            req.arrive_by = false;
        } else {
            fprintf(stderr, "Only Arrive or Depart please.\n");
            exit(-1);
        }
    } else {
        req.arrive_by = false;
    }
    
    if (argc >= 5) {
        struct tm ltm;
        memset(&ltm, 0, sizeof(struct tm));
        strptime(argv[4], "%Y-%m-%dT%H:%M:%S", &ltm);
        req.time = mktime(&ltm);
    }

    /* SETUP */
    
    // load transit data from disk
    tdata_t tdata;
    tdata_load((argc < 6 ? RRRR_INPUT_FILE : argv[5]), &tdata);

    if (req.from >= tdata.n_stops || req.to >= tdata.n_stops) {   
        fprintf(stderr, "Invalid stopids in from and/or to.\n");
        exit(-1);
    }

    // initialize router
    router_t router;
    router_setup(&router, &tdata);
    //tdata_dump(&tdata); // debug timetable file format

    char result_buf[8000];
    router_request_dump (&router, &req);
    router_route (&router, &req);
    router_result_dump(&router, &req, result_buf, 8000);
    printf("%s", result_buf);
    // repeat search in reverse to compact transfers
    uint32_t n_reversals = req.arrive_by ? 1 : 2;
    // n_reversals = 0; // DEBUG turn off reversals
    for (uint32_t i = 0; i < n_reversals; ++i) {
        router_request_reverse (&router, &req); // handle case where route is not reversed
        printf ("Repeating search with reversed request: \n");
        router_request_dump (&router, &req);
        router_route (&router, &req);
        router_result_dump(&router, &req, result_buf, 8000);
        printf("%s", result_buf);
    }
    
    tdata_close(&tdata);
    exit(EXIT_SUCCESS);
}

