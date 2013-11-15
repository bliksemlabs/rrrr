/* tester.c : single-theaded test of router for unit tests and debugging */

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
#include "parse.h"
#include "json.h"

// consider changing to --arrive TIME or --depart TIME --date DATE
// also look up stop ids

#define OUTPUT_LEN 8000

static struct option long_options[] = {
    { "arrive",        no_argument, NULL, 'a' },
    { "depart",        no_argument, NULL, 'd' },
    { "random",        no_argument, NULL, 'r' },
    { "help",          no_argument, NULL, 'h' },
    { "date",          required_argument, NULL, 'D' },
    { "walk-slack",    required_argument, NULL, 's' },
    { "walk-speed",    required_argument, NULL, 'S' },
    { "optimise",      required_argument, NULL, 'o' },
    { "from-idx",      required_argument, NULL, 'f' },
    { "to-idx",        required_argument, NULL, 't' },
    { "via-idx",       required_argument, NULL, 'V' },
    { "mode",          required_argument, NULL, 'm' },
    { "start-trip-idx",    required_argument, NULL, 'Q' },
    { "banned-routes-idx", required_argument, NULL, 'x' },
    { "banned-stops-idx",  required_argument, NULL, 'y' },
    { "banned-trips-idx",  required_argument, NULL, 'z' },
    { "banned-stops-hard-idx",  required_argument, NULL, 'w' },
    { "trip-attributes", required_argument, NULL, 'A' },
    { "gtfsrt",        required_argument, NULL, 'g' },
    { "gtfsrt-alerts", required_argument, NULL, 'G' },
    { "timetable",     required_argument, NULL, 'T' },
    { "verbose",     no_argument, NULL, 'v' },
    { NULL, 0, 0, 0 } /* end */
};

int main(int argc, char **argv) {

    router_request_t req;
    router_request_initialize (&req);

    char *tdata_file = RRRR_INPUT_FILE;
    char *gtfsrt_file = NULL;
    char *gtfsrt_alerts_file = NULL;
    bool verbose = false;

    int opt = 0;
    while (opt >= 0) {
        opt = getopt_long(argc, argv, "adrhD:s:S:o:f:t:V:m:Q:x:y:z:w:A:g:G:T:v", long_options, NULL);
        if (opt < 0) continue;
        switch (opt) {
        case 'T':
            tdata_file = optarg;
            break;
        case 'g':
            gtfsrt_file = optarg;
            break;
        case 'G':
            gtfsrt_alerts_file = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        case 'h':
            goto usage;
        }
    }

    /* SETUP */

    // load transit data from disk
    tdata_t tdata;
    tdata_load(tdata_file, &tdata);
    optind = 0;
    opt = 0;
    while (opt >= 0) {
        opt = getopt_long(argc, argv, "adrhD:s:S:o:f:t:V:m:Q:x:y:z:w:A:g:G:T:v", long_options, NULL);
        parse_request(&req, &tdata, NULL, opt, optarg);
    }

    if (req.from == NONE || req.to == NONE) goto usage;

    if (req.from == req.to) {
        fprintf(stderr, "Dude, you are already there.\n");
        exit(-1);
    }

    if (req.from >= tdata.n_stops || req.to >= tdata.n_stops) {
        fprintf(stderr, "Invalid stopids in from and/or to.\n");
        exit(-1);
    }

    // load gtfs-rt file from disk
    if (gtfsrt_file != NULL || gtfsrt_alerts_file != NULL) {
        RadixTree *tripid_index  = rxt_load_strings_from_tdata (tdata.trip_ids, tdata.trip_id_width, tdata.n_trips);
        if (gtfsrt_file != NULL) {
            tdata_clear_gtfsrt (&tdata);
            tdata_apply_gtfsrt_file (&tdata, tripid_index, gtfsrt_file);
        }

        if (gtfsrt_alerts_file != NULL) {
            RadixTree *routeid_index = rxt_load_strings_from_tdata (tdata.route_ids, tdata.route_id_width, tdata.n_routes);
            RadixTree *stopid_index  = rxt_load_strings_from_tdata (tdata.stop_ids, tdata.stop_id_width, tdata.n_stops);
            tdata_clear_gtfsrt_alerts(&tdata);
            tdata_apply_gtfsrt_alerts_file (&tdata, routeid_index, stopid_index, tripid_index, gtfsrt_alerts_file);
        }
    }

    // initialize router
    router_t router;
    router_setup(&router, &tdata);
    //tdata_dump(&tdata); // debug timetable file format

    char result_buf[OUTPUT_LEN];
    router_route (&router, &req);
    if (verbose) {
        router_request_dump (&router, &req);
        router_result_dump(&router, &req, result_buf, OUTPUT_LEN);
        printf("%s", result_buf);
    }
    // repeat search in reverse to compact transfers
    uint32_t n_reversals = req.arrive_by ? 1 : 2;
    // but do not reverse requests starting on board (they cannot be compressed, earliest arrival is good enough)
    if (req.start_trip_trip != NONE) n_reversals = 0;
    // n_reversals = 0; // DEBUG turn off reversals
    for (uint32_t i = 0; i < n_reversals; ++i) {
        router_request_reverse (&router, &req); // handle case where route is not reversed
        router_route (&router, &req);
        if (verbose) {
            printf ("Repeated search with reversed request: \n");
            router_request_dump (&router, &req);
            router_result_dump(&router, &req, result_buf, OUTPUT_LEN);
            printf("%s", result_buf);
        }
    }

    /* Output only final result in non-verbose mode */
    if (!verbose) {
        router_result_dump(&router, &req, result_buf, OUTPUT_LEN);
        printf("%s", result_buf);
    }

    tdata_close(&tdata);
    router_teardown(&router);

    exit(EXIT_SUCCESS);

    usage:
    printf("Usage:\n%s [-r(andomize)] [-from-idx from_stop] [-to-idx to_stop] [-a(rrive)] [-d(epart)] [-D YYYY-MM-DDThh:mm:ss] [-g gtfsrt.pb] [-T timetable.dat]\n", argv[0]);
    exit(-2);
}

