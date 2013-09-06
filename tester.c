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
    { "from-idx", required_argument, NULL, 'f' },
    { "to-idx",   required_argument, NULL, 't' },
    { "mode",   required_argument, NULL, 'm' },
    { "banned-routes", required_argument, NULL, 'x' },
    { "banned-stops", required_argument, NULL, 'y' },
    { "gtfsrt", required_argument, NULL, 'g' },
    { "timetable", required_argument, NULL, 'T' },
    { NULL, 0, 0, 0 } /* end */
};

int main(int argc, char **argv) {

    router_request_t req;
    router_request_initialize (&req);

    char *tdata_file  = RRRR_INPUT_FILE;
    char *gtfsrt_file = NULL;

    const char delim[2] = ",";
    char *token;
    
    struct tm ltm;
    int opt = 0;
    while (opt >= 0) {
        opt = getopt_long(argc, argv, "adrhD:f:t:m:g:T:", long_options, NULL);
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
            memset(&ltm, 0, sizeof(struct tm));
            strptime(optarg, "%Y-%m-%dT%H:%M:%S", &ltm);
            req.time = mktime(&ltm);
            break;
        case 'f':
            req.from = strtol(optarg, NULL, 10);
            break;
        case 't':
            req.to = strtol(optarg, NULL, 10);
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
        case 'x':
            req.n_banned_routes = 1;
            for (int i = 0; i < strlen(optarg); i++) {
                if (optarg[i] == ',') req.n_banned_routes++;
            }
            req.banned_routes = (uint32_t *) calloc(req.n_banned_routes, sizeof(uint32_t));

            req.n_banned_routes = 0;
            token = strtok(optarg, delim);
            while ( token  != NULL ) {
                if (strlen(token) > 0) {
                    long int tmp = strtol(token, NULL, 10);
                    if (tmp > 0) {
                        req.banned_routes[req.n_banned_routes] = tmp;
                        req.n_banned_routes++;
                    }
                }

                token = strtok(NULL, delim);
            }
            break;
        case 'y':
            req.n_banned_stops = 1;
            for (int i = 0; i < strlen(optarg); i++) {
                if (optarg[i] == ',') req.n_banned_stops++;
            }
            req.banned_stops = (uint32_t *) calloc(req.n_banned_stops, sizeof(uint32_t));

            req.n_banned_stops = 0;
            token = strtok(optarg, delim);
            while ( token  != NULL ) {
                if (strlen(token) > 0) {
                    long int tmp = strtol(token, NULL, 10);
                    if (tmp > 0) {
                        req.banned_stops[req.n_banned_stops] = tmp;
                        req.n_banned_stops++;
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

    if (req.banned_routes)
        free(req.banned_routes);

    exit(EXIT_SUCCESS);
    
    usage:
    printf("Usage:\n%stesterrrr [-r(andomize)] [-f from_stop] [-t to_stop] [-a(rrive)] [-d(epart)] [-D YYYY-MM-DDThh:mm:ss] [-g gtfsrt.pb] [-T timetable.dat]\n", argv[0]);
    exit(-2);
}

