/* tester.c : single-theaded test of router for unit tests and debugging */

#define _GNU_SOURCE

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "rrrr.h"
#include "tdata.h"
#include "router.h"

// consider changing to --arrive TIME or --depart TIME --date DATE
// also look up stop ids

static struct option long_options[] = {
    { "arrive", no_argument, NULL, 'a' },
    { "depart", no_argument, NULL, 'd' },
    { "random", no_argument, NULL, 'r' },
    { "help",   no_argument, NULL, 'h' },
    { "date",   required_argument, NULL, 'D' },
    { "from",   required_argument, NULL, 'f' },
    { "to",     required_argument, NULL, 't' },
    { "gtfsrt", required_argument, NULL, 'g' },
    { "timetable", required_argument, NULL, 'T' },
    { NULL, 0, 0, 0 } /* end */
};

int main(int argc, char **argv) {

    router_request_t req;
    router_request_initialize (&req);

    char *tdata_file = RRRR_INPUT_FILE;
    char *gtfsrt_file = NULL;
    char *iso_datetime = NULL;
    
    int opt = 0;
    while (opt >= 0) {
        opt = getopt_long(argc, argv, "adrhD:f:t:g:T:", long_options, NULL);
        if (opt < 0) continue;
        switch (opt) {
        case 'a':
            req.arrive_by = true;
            break;
        case 'd':
            req.arrive_by = false;
            break;
        case 'r':
            srand(time(NULL));
            router_request_randomize(&req);
            break;
        case 'h':
            goto usage;
        case 'D':
            iso_datetime = optarg;
            break;
        case 'f':
            req.from = strtol(optarg, NULL, 10);
            break;
        case 't':
            req.to = strtol(optarg, NULL, 10);
            break;
        case 'T':
            tdata_file = optarg;
            break;
        case 'g':
            gtfsrt_file = optarg;
            break;
        }
    }
    
    if (req.from == NONE || req.to == NONE) goto usage;
    
    if (req.from == req.to) {  
        fprintf(stderr, "Dude, you are already there.\n");
        exit(-1);
    }


    /* SETUP */
    
    // load transit data from disk
    tdata_t tdata;
    tdata_load(tdata_file, &tdata);

    if (req.from >= tdata.n_stops || req.to >= tdata.n_stops) {   
        fprintf(stderr, "Invalid stopids in from and/or to.\n");
        exit(-1);
    }

    // load gtfs-rt file from disk
    if (gtfsrt_file != NULL) {
        RadixTree *tripid_index = rxt_load_strings ("trips");
        tdata_clear_gtfsrt (&tdata);
        tdata_apply_gtfsrt_file (&tdata, tripid_index, gtfsrt_file);
    }

    if (iso_datetime != NULL) {
        struct tm ltm;
        memset (&ltm, 0, sizeof(struct tm));
        strptime (iso_datetime, "%Y-%m-%dT%H:%M:%S", &ltm);
        router_request_from_epoch (&req, &tdata, mktime(&ltm)); // from struct_tm instead?
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
    
    usage:
    printf("Usage:\n%stesterrrr [-r(andomize)] [-f from_stop] [-t to_stop] [-a(rrive)] [-d(epart)] [-D YYYY-MM-DDThh:mm:ss] [-g gtfsrt.pb] [-T timetable.dat]\n", argv[0]);
    exit(-2);
}

