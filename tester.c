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

// consider changing to --arrive TIME or --depart TIME --date DATE
// also look up stop ids

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
    { "mode",          required_argument, NULL, 'm' },
    { "banned-routes-idx", required_argument, NULL, 'x' },
    { "banned-stops-idx",  required_argument, NULL, 'y' },
    { "trip-attributes", required_argument, NULL, 'A' },
    { "gtfsrt",        required_argument, NULL, 'g' },
    { "timetable",     required_argument, NULL, 'T' },
    { "verbose",     no_argument, NULL, 'v' },
    { NULL, 0, 0, 0 } /* end */
};

int main(int argc, char **argv) {

    router_request_t req;
    router_request_initialize (&req);

    char *tdata_file = RRRR_INPUT_FILE;
    char *gtfsrt_file = NULL;
    char *iso_datetime = NULL;
    bool verbose = false;

    const char delim[2] = ",";
    char *token;
    
    int opt = 0;
    while (opt >= 0) {
        opt = getopt_long(argc, argv, "adrhD:s:S:o:f:t:m:x:y:A:g:T:v", long_options, NULL);
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
        case 's':
            req.walk_slack = strtol(optarg, NULL, 10);
            break;
        case 'S':
            req.walk_speed = strtod(optarg, NULL);
            break;
        case 'o':
            req.optimise = 0;
            token = strtok(optarg, delim);

            while ( token != NULL ) {
                if (strcmp(token, "shortest")  == 0) req.optimise |= o_shortest;
                if (strcmp(token, "transfers") == 0) req.optimise |= o_transfers;
                if (strcmp(token, "all")       == 0) req.optimise  = o_all;
                token = strtok(NULL, delim);
            }
            break;
        case 'm':
            req.mode = 0;
            token = strtok(optarg, delim);

            while ( token  != NULL ) {
                if (strcmp(token, "tram") == 0)      req.mode |= m_tram;
                if (strcmp(token, "subway") == 0)    req.mode |= m_subway;
                if (strcmp(token, "rail") == 0)      req.mode |= m_rail;
                if (strcmp(token, "bus") == 0)       req.mode |= m_bus;
                if (strcmp(token, "ferry") == 0)     req.mode |= m_ferry;
                if (strcmp(token, "cablecar") == 0)  req.mode |= m_cablecar;
                if (strcmp(token, "gondola") == 0)   req.mode |= m_gondola;
                if (strcmp(token, "funicular") == 0) req.mode |= m_funicular;
                if (strcmp(token, "all") == 0)       req.mode = m_all;

                token = strtok(NULL, delim);
            }
            break;
        case 'A':
            token = strtok(optarg, delim);

            while ( token != NULL ) {
                if (strcmp(token, "accessible") == 0) req.trip_attributes |= ta_accessible;
                if (strcmp(token, "toilet")     == 0) req.trip_attributes |= ta_toilet;
                if (strcmp(token, "wifi")       == 0) req.trip_attributes |= ta_wifi;
                if (strcmp(token, "none")       == 0) req.trip_attributes =  ta_none;
                token = strtok(NULL, delim);
            }
            break;
        case 'x':
            token = strtok(optarg, delim);
            while ( token  != NULL ) {
                if (strlen(token) > 0) {
                    long int tmp = strtol(token, NULL, 10);
                    if (tmp > 0) {
                        req.banned_route = tmp;
                        req.n_banned_routes = 1;
                    }
                }

                token = strtok(NULL, delim);
            }
            break;
        case 'y':
            token = strtok(optarg, delim);
            while ( token  != NULL ) {
                if (strlen(token) > 0) {
                    long int tmp = strtol(token, NULL, 10);
                    if (tmp > 0) {
                        req.banned_stop = tmp;
                        req.n_banned_stops = 1;
                    }
                }

                token = strtok(NULL, delim);
            }
            break;
        case 'T':
            tdata_file = optarg;
            break;
        case 'g':
            gtfsrt_file = optarg;
            break;
        case 'v':
            verbose = true;
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
    router_route (&router, &req);
    if (verbose) {
        router_request_dump (&router, &req);
        router_result_dump(&router, &req, result_buf, 8000);
        printf("%s", result_buf);
    }
    // repeat search in reverse to compact transfers
    uint32_t n_reversals = req.arrive_by ? 1 : 2;
    // n_reversals = 0; // DEBUG turn off reversals
    for (uint32_t i = 0; i < n_reversals; ++i) {
        router_request_reverse (&router, &req); // handle case where route is not reversed
        router_route (&router, &req);
        if (verbose) {
            printf ("Repeated search with reversed request: \n");
            router_request_dump (&router, &req);
            router_result_dump(&router, &req, result_buf, 8000);
            printf("%s", result_buf);
        }
    }

    /* Output only final result in non-verbose mode */
    if (!verbose) {
        router_result_dump(&router, &req, result_buf, 8000);
        printf("%s", result_buf);
    }    
    
    tdata_close(&tdata);

    exit(EXIT_SUCCESS);
    
    usage:
    printf("Usage:\n%stesterrrr [-r(andomize)] [-f from_stop] [-t to_stop] [-a(rrive)] [-d(epart)] [-D YYYY-MM-DDThh:mm:ss] [-g gtfsrt.pb] [-T timetable.dat]\n", argv[0]);
    exit(-2);
}

