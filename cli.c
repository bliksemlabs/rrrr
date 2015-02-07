/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

/* cli.c : single-threaded commandline interface to the library */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "api.h"
#include "set.h"
#include "router_request.h"
#include "router_result.h"
#include "plan_render_text.h"

#ifdef RRRR_FEATURE_REALTIME

#ifdef RRRR_FEATURE_REALTIME_ALERTS
#include "tdata_realtime_alerts.h"
#endif

#ifdef RRRR_FEATURE_REALTIME_EXPANDED
#include "tdata_realtime_expanded.h"
#endif

#endif

#define OUTPUT_LEN 8000

typedef struct cli_arguments cli_arguments_t;
struct cli_arguments {
    char *gtfsrt_alerts_filename;
    char *gtfsrt_tripupdates_filename;
    long repeat;
    bool verbose;
    #ifdef RRRR_FEATURE_LATLON
    bool has_latlon;
    #endif
};

int main (int argc, char *argv[]) {
    /* our return value */
    int status = EXIT_SUCCESS;

    /* our commandline arguments */
    cli_arguments_t cli_args;

    /* the request structure, includes the input parameters to the router */
    router_request_t req;

    /* the timetable structure, the interface to the timetable */
    tdata_t tdata;

    /* the router structure, should not be manually changed */
    router_t router;

    /* the plan structure, created for all the results */
    plan_t plan;

    /* initialise the structs so we can always trust NULL values */
    memset (&tdata,    0, sizeof(tdata_t));
    memset (&router,   0, sizeof(router_t));
    memset (&cli_args, 0, sizeof(cli_args));

    plan.n_itineraries = 0;

    /* * * * * * * * * * * * * * * * * * * * * *
     * PHASE ZERO: HANDLE COMMANDLINE ARGUMENTS
     *
     * * * * * * * * * * * * * * * * * * * * * */

    if (argc < 3) {
        fprintf(stderr, "Usage:\n%s timetable.dat\n"
                        "[ --verbose ] [ --randomize ]\n"
                        "[ --arrive=YYYY-MM-DDTHH:MM:SS | "
                          "--depart=YYYY-MM-DDTHH:MM:SS ]\n"
                        "[ --from-idx=idx | --from-id=id | --from-latlon=Y,X ]\n"
                        "[ --via-idx=idx  | --via-id=id  | --via-latlon=Y,X ]\n"
                        "[ --to-idx=idx   | --to-id=id   | --to-latlon=Y,X ]\n"
#if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
                        "[ --banned-jp-idx=idx ]\n"
#endif
#if RRRR_MAX_BANNED_STOP_POINTS > 0
                        "[ --banned-stop-idx=idx ]\n"
#endif
#if RRRR_MAX_BANNED_STOP_HARD > 0
                        "[ --banned-stop-hard-idx=idx ]\n"
#endif
#if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
                        "[ --banned-vj-offset=jp_idx,trip_offset ]\n"
#endif
#if RRRR_FEATURE_REALTIME_ALERTS == 1
                        "[ --gtfsrt-alerts=filename.pb ]\n"
#endif
#if RRRR_FEATURE_REALTIME_EXPANDED == 1
                        "[ --gtfsrt-tripupdates=filename.pb ]\n"
#endif
                        "[ --repeat=n ]\n"
                        , argv[0]);
    }

    /* The first mandartory argument is the timetable file. We must initialise
     * the timetable prior to parsing so we are able to check the validity of
     * the input.
     *
     * The tdata_file stores timetable data in binary format.
     * We have several ways to load this data from disk;
     *
     * 1) memory mapped (tdata_io_v3_mmap.c)
     * Pro: The operation is resource efficient
     * Con: The operating system has to support it,
     *      the data can't be arbitrary extended
     *
     * 2) using read into dynamically allocated memory (tdata_io_v3_dynamic.c)
     * Pro: This will work in all C implementations
     * Con: Requires heap memory which stores the timetable in full
     */

    if ( ! tdata_load (&tdata, argv[1])) {
        /* if the data can't be loaded we must exit */
        status = EXIT_FAILURE;
        goto clean_exit;
    }

    /* initialise the request */
    router_request_initialize (&req);

    /* initialise the random function */
    srand((unsigned int) time(NULL));


    {
        int i;
        for (i = 1; i < argc; i++) {
            if (argv[i + 0][0] == '-' && argv[i + 0][1] == '-') {
                /* all arguments will be in the form --argument= */

                switch (argv[i][2]) {
                case 'a':
                    if (strncmp(argv[i], "--arrive=", 9) == 0) {
                        router_request_from_epoch (&req, &tdata,
                                                   strtoepoch(&argv[i][9]));
                        req.arrive_by = true;
                    }
                    break;

                case 'd':
                    if (strncmp(argv[i], "--depart=", 9) == 0) {
                        router_request_from_epoch (&req, &tdata,
                                                   strtoepoch(&argv[i][9]));
                        req.arrive_by = false;
                    }
                    break;

                case 'f':
                    if (strncmp(argv[i], "--from-idx=", 11) == 0) {
                        strtospidx (&argv[i][11], &tdata, &req.from_stop_point, NULL);
                    }
                    else if (strncmp(argv[i], "--from-id=", 10) == 0) {
                        req.from_stop_point = tdata_stop_pointidx_by_stop_point_id (&tdata, &argv[i][10], 0);
                    }
                    #ifdef RRRR_FEATURE_LATLON
                    else if (strncmp(argv[i], "--from-latlon=", 14) == 0) {
                        cli_args.has_latlon = strtolatlon(&argv[i][14], &req.from_latlon);
                    }
                    #endif
                    break;

                #ifdef RRRR_FEATURE_REALTIME
                case 'g':
                    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
                    if (strncmp(argv[i], "--gtfsrt-tripupdates=", 21) == 0) {
                        cli_args.gtfsrt_tripupdates_filename = &argv[i][21];
                    }
                    #endif
                    #ifdef RRRR_FEATURE_REALTIME_ALERTS
                    if (strncmp(argv[i], "--gtfsrt-alerts=", 16) == 0) {
                        cli_args.gtfsrt_alerts_filename = &argv[i][16];
                    }
                    #endif
                    break;
                #endif

                case 'r':
                    if (strcmp(argv[i], "--randomize") == 0) {
                        router_request_randomize (&req, &tdata);
                    }
                    else if (strncmp(argv[i], "--repeat=", 9) == 0) {
                        cli_args.repeat = strtol(&argv[i][9], NULL, 10);
                    }
                    break;

                case 't':
                    if (strncmp(argv[i], "--to-idx=", 9) == 0) {
                        strtospidx (&argv[i][9], &tdata, &req.to_stop_point, NULL);
                    }
                    else if (strncmp(argv[i], "--to-id=", 8) == 0) {
                        req.to_stop_point = tdata_stop_pointidx_by_stop_point_id (&tdata, &argv[i][8], 0);
                    }
                    #ifdef RRRR_FEATURE_LATLON
                    else if (strncmp(argv[i], "--to-latlon=", 12) == 0) {
                        cli_args.has_latlon = strtolatlon(&argv[i][12], &req.to_latlon);
                    }
                    #endif
                    break;

                case 'b':
                    if (false) {}
                    #if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
                    else
                    if (strncmp(argv[i], "--banned-jp-idx=", 16) == 0) {
                        jpidx_t jp;
                        if (strtojpidx (&argv[i][16], &tdata, &jp, NULL)) {
                            set_add_jp(req.banned_journey_patterns,
                                       &req.n_banned_journey_patterns,
                                       RRRR_MAX_BANNED_JOURNEY_PATTERNS,
                                       jp);
                        }
                    }
                    #endif
                    #if RRRR_MAX_BANNED_STOP_POINTS > 0
                    else
                    if (strncmp(argv[i], "--banned-stop-idx=", 19) == 0) {
                        spidx_t sp;
                        if (strtospidx (&argv[i][19], &tdata, &sp, NULL)) {
                            set_add_sp(req.banned_stops,
                                       &req.n_banned_stops,
                                       RRRR_MAX_BANNED_STOP_POINTS,
                                       sp);
                        }
                    }
                    #endif
                    #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
                    else
                    if (strncmp(argv[i], "--banned-stop-hard-idx=", 23) == 0) {
                        spidx_t sp;
                        if (strtospidx (&argv[i][23], &tdata, &sp, NULL)) {
                             set_add_sp(req.banned_stop_points_hard,
                                       &req.n_banned_stop_points_hard,
                                       RRRR_MAX_BANNED_STOP_POINTS_HARD,
                                       sp);
                        }
                    }
                    #endif
                    #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
                    else
                    if (strncmp(argv[i], "--banned-vj-offset=", 19) == 0) {
                        char *endptr;
                        jpidx_t jp;
                        if (strtojpidx (&argv[i][21], &tdata, &jp, &endptr)) {
                            jp_vjoffset_t vj_o;
                            if (endptr[0] == ',' &&
                                strtovjoffset (++endptr, &tdata, jp, &vj_o, NULL)) {
                                set_add_vj(req.banned_vjs_journey_pattern,
                                           req.banned_vjs_offset,
                                           &req.n_banned_vjs,
                                           RRRR_MAX_BANNED_VEHICLE_JOURNEYS,
                                           jp, vj_o);
                            }
                        }
                    }
                    #endif
                    break;

                case 'v':
                    if (strcmp(argv[i], "--verbose") == 0) {
                        cli_args.verbose = true;
                    }
                    else if (strncmp(argv[i], "--via-idx=", 10) == 0) {
                        strtospidx (&argv[i][10], &tdata, &req.via_stop_point, NULL);
                    }
                    else if (strncmp(argv[i], "--via-id=", 9) == 0) {
                        req.via_stop_point = tdata_stop_pointidx_by_stop_point_id (&tdata, &argv[i][9], 0);
                    }
                    #ifdef RRRR_FEATURE_LATLON
                    else if (strncmp(argv[i], "--via-latlon=", 13) == 0) {
                        cli_args.has_latlon = strtolatlon(&argv[i][13], &req.via_latlon);
                    }
                    #endif
                    break;

                case 'w':
                    if (strncmp(argv[i], "--walk-speed=", 13) == 0) {
                        req.walk_speed = (float) strtod(&argv[i][13], NULL);
                    }
                    else if (strncmp(argv[i], "--walk-slack=", 13) == 0) {
                        long walk_slack = strtol(&argv[i][13], NULL, 10);
                        if (walk_slack >= 0 && walk_slack <= 255) {
                            req.walk_slack = (uint8_t) walk_slack;
                        }
                    }
                    break;

                default:
                    fprintf(stderr, "Unknown option: %s\n", argv[i]);
                    break;
                }
            }
        }
    }

    /* */

    #ifdef RRRR_FEATURE_REALTIME
    if (cli_args.gtfsrt_alerts_filename != NULL ||
        cli_args.gtfsrt_tripupdates_filename != NULL) {

        if (!tdata_realtime_setup (&tdata)) {
            status = EXIT_FAILURE;
            goto clean_exit;
        }

        #ifdef RRRR_FEATURE_REALTIME_ALERTS
        if (cli_args.gtfsrt_alerts_filename != NULL) {
            tdata_apply_gtfsrt_alerts_file (&tdata, cli_args.gtfsrt_alerts_filename);
        }
        #endif
        #ifdef RRRR_FEATURE_REALTIME_EXPANDED
        if (cli_args.gtfsrt_tripupdates_filename != NULL) {
            tdata_apply_gtfsrt_tripupdates_file (&tdata, cli_args.gtfsrt_tripupdates_filename);
        }
        #endif
    }
    #endif

    /* The internal time representation uses a resolution of 4 seconds per
     * time unit. This allows to store up to 3 days in 16 bits. When a user
     * specified a clockwise search departing after 12:00:03, this would
     * actually be interpreted as 12:00:00.
     */

    if (req.time_rounded && ! (req.arrive_by)) {
        req.time++;
    }
    req.time_rounded = false;

    #ifdef RRRR_FEATURE_LATLON
    if (cli_args.has_latlon && ! tdata_hashgrid_setup (&tdata)) {
        status = EXIT_FAILURE;
        goto clean_exit;
    }
    #endif

   /* * * * * * * * * * * * * * * * * *
     * PHASE ONE: INITIALISE THE ROUTER
     *
     * * * * * * * * * * * * * * * * * */

    /* The router structure stores all properties and allocated memory
     * required for a search operation.
     * Secondary it initialises the hashgrid used for distance calculations.
     *
     * The contents of this struct MUST NOT be changed directly.
     */
    if ( ! router_setup (&router, &tdata)) {
        /* if the memory is not allocated we must exit */
        status = EXIT_FAILURE;
        goto clean_exit;
    }

    /* * * * * * * * * * * * * * * * * * *
     *  PHASE TWO: DO THE SEARCH
     *
     * * * * * * * * * * * * * * * * * * */

    /* Reset the cutoff time to UNREACHED or 0 to simulate a complete new request,
     * this erases the set cutoff time from reversals in previous requests in the repeat function
     */
    if (req.arrive_by) {
        req.time_cutoff = 0;
    } else {
        req.time_cutoff = UNREACHED;
    }

    /* The router is now able to take a request, and to search
     * the first arrival time at the target, given the requests
     * origin.
     */
    if ( ! router_route_full_reversal (&router, &req, &plan) ) {
        status = EXIT_FAILURE;
        goto clean_exit;
    }

    /* * * * * * * * * * * * * * * * * * *
     *  PHASE THREE: RENDER THE RESULTS
     *
     * * * * * * * * * * * * * * * * * * */
    {
        char result_buf[OUTPUT_LEN];
        plan.req = req;
        plan_render_text (&plan, &tdata, result_buf, OUTPUT_LEN);
        puts(result_buf);
    }

    /* * * * * * * * * * * * * * * * * * *
     *  PHASE FOUR: DESTRUCTION
     *
     * * * * * * * * * * * * * * * * * * */

    /* The operation system takes care for our deallocation when we exit the
     * application. For debugging purposes we want to be completely clear about
     * our memory allocation and deallocation.
     */

clean_exit:
    #ifndef RRRR_VALGRIND
    goto fast_exit;
    #endif

    /* Deallocate the scratchspace of the router */
    router_teardown (&router);

    /* Unmap the memory and/or deallocate the memory on the heap */
    tdata_close (&tdata);

    #ifdef RRRR_VALGRIND
    goto fast_exit; /* kills the unused label warning */
    #endif

fast_exit:
    exit(status);
}
