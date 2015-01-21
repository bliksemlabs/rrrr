/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

/* cli.c : single-threaded commandline interface to the library */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "router_request.h"
#include "router_result.h"

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
    uint32_t repeat;
    bool verbose;
};

#if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
static void set_add_jp (uint32_t *set,
                        uint8_t  *length, uint8_t max_length,
                        uint32_t value) {
    uint8_t i;

    if (*length >= max_length) return;

    for (i = 0; i < *length; ++i) {
        if (set[i] == value) return;
    }

    set[*length] = value;
    (*length)++;
}
#endif

#if RRRR_MAX_BANNED_STOP_POINTS > 0 || RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
static void set_add_sp (spidx_t *set,
                        uint8_t  *length, uint8_t max_length,
                        spidx_t value) {
    uint8_t i;

    if (*length >= max_length) return;

    for (i = 0; i < *length; ++i) {
        if (set[i] == value) return;
    }

    set[*length] = value;
    (*length)++;
}
#endif


#if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
static void set_add_trip (uint32_t *set1, uint16_t *set2,
                          uint8_t  *length, uint8_t max_length,
                          uint32_t value1, uint16_t value2) {
    uint8_t i;

    if (*length >= max_length) return;

    for (i = 0; i < *length; ++i) {
        if (set1[i] == value1 &&
            set2[i] == value2) return;
    }

    set1[*length] = value1;
    set2[*length] = value2;
    (*length)++;
}
#endif

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

    /* initialise the structs so we can always trust NULL values */
    memset (&tdata,    0, sizeof(tdata_t));
    memset (&router,   0, sizeof(router_t));
    memset (&cli_args, 0, sizeof(cli_args));

    /* * * * * * * * * * * * * * * * * * * * * *
     * PHASE ZERO: HANDLE COMMANDLINE ARGUMENTS
     *
     * * * * * * * * * * * * * * * * * * * * * */

    if (argc < 3) {
        fprintf(stderr, "Usage:\n%s timetable.dat\n"
                        "[ --verbose ] [ --randomize ]\n"
                        "[ --arrive=YYYY-MM-DDTHH:MM:SS | "
                          "--depart=YYYY-MM-DDTHH:MM:SS ]\n"
                        "[ --from-idx=idx | --from-latlon=Y,X ]\n"
                        "[ --via-idx=idx  | --via-latlon=Y,X ]\n"
                        "[ --to-idx=idx   | --to-latlon=Y,X ]\n"
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
    srand(time(NULL));


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
                        req.from_stop_point = (uint32_t) strtol(&argv[i][11], NULL, 10);
                    }
                    #ifdef RRRR_FEATURE_LATLON
                    else if (strncmp(argv[i], "--from-latlon=", 14) == 0) {
                        /* TODO: check return value */
                        strtolatlon(&argv[i][14], &req.from_latlon);
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
                        cli_args.repeat = (uint32_t) strtol(&argv[i][9], NULL, 10);
                    }
                    break;

                case 't':
                    if (strncmp(argv[i], "--to-idx=", 9) == 0) {
                        req.to_stop_point = (uint32_t) strtol(&argv[i][9], NULL, 10);
                    }
                    #ifdef RRRR_FEATURE_LATLON
                    else if (strncmp(argv[i], "--to-latlon=", 12) == 0) {
                        /* TODO: check return value */
                        strtolatlon(&argv[i][12], &req.to_latlon);
                    }
                    #endif
                    break;

                case 'b':
                    if (false) {}
                    #if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
                    else
                    if (strncmp(argv[i], "--banned-jp-idx=", 16) == 0) {
                        uint32_t jp_index = (uint32_t) strtol(&argv[i][16], NULL, 10);
                        if (jp_index < tdata.n_journey_patterns) {
                            set_add_jp(req.banned_journey_patterns,
                                       &req.n_banned_journey_patterns,
                                       RRRR_MAX_BANNED_JOURNEY_PATTERNS,
                                       jp_index);
                        }
                    }
                    #endif
                    #if RRRR_MAX_BANNED_STOP_POINTS > 0
                    else
                    if (strncmp(argv[i], "--banned-stop-idx=", 19) == 0) {
                        uint32_t stop_idx = (uint32_t) strtol(&argv[i][19], NULL, 10);
                        if (stop_idx < tdata.n_stop_points) {
                            set_add_sp(req.banned_stops,
                                       &req.n_banned_stops,
                                    RRRR_MAX_BANNED_STOP_POINTS,
                                       stop_idx);
                        }
                    }
                    #endif
                    #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
                    else
                    if (strncmp(argv[i], "--banned-stop-hard-idx=", 23) == 0) {
                        uint32_t stop_idx = (uint32_t) strtol(&argv[i][23], NULL, 10);
                        if (stop_idx < tdata.n_stop_points) {
                            set_add_sp(req.banned_stop_points_hard,
                                       &req.n_banned_stop_points_hard,
                                    RRRR_MAX_BANNED_STOP_POINTS_HARD,
                                       stop_idx);
                        }
                    }
                    #endif
                    #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
                    else
                    if (strncmp(argv[i], "--banned-vj-offset=", 19) == 0) {
                        char *endptr;
                        uint32_t jp_index;

                        jp_index = (uint32_t) strtol(&argv[i][21], &endptr, 10);
                        if (jp_index < tdata.n_journey_patterns && endptr[0] == ',') {
                            uint16_t vj_offset = strtol(++endptr, NULL, 10);
                            if (vj_offset < tdata.journey_patterns[jp_index].n_vjs) {
                                set_add_trip(req.banned_vjs_journey_pattern,
                                             req.banned_vjs_offset,
                                             &req.n_banned_vjs,
                                        RRRR_MAX_BANNED_VEHICLE_JOURNEYS,
                                             jp_index, vj_offset);
                            }
                        }
                    }
                    #endif
                    break;

                case 'v':
                    if (strcmp(argv[i], "--verbose") == 0) {
                        cli_args.verbose = true;
                    }
                    else if (strncmp(argv[i], "--via=", 6) == 0) {
                        req.via_stop_point = (uint32_t) strtol(&argv[i][6], NULL, 10);
                    }
                    #ifdef RRRR_FEATURE_LATLON
                    else if (strncmp(argv[i], "--via-latlon=", 13) == 0) {
                        /* TODO: check return value */
                        strtolatlon(&argv[i][13], &req.via_latlon);
                    }
                    #endif
                    break;

                case 'w':
                    if (strncmp(argv[i], "--walk-speed=", 13) == 0) {
                        req.walk_speed = (float) strtod(&argv[i][13], NULL);
                    }
                    else if (strncmp(argv[i], "--walk-slack=", 13) == 0) {
                        req.walk_slack = (uint8_t) strtod(&argv[i][13], NULL);
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

        tdata.stop_point_id_index = radixtree_load_strings_from_tdata (tdata.stop_point_ids, tdata.stop_point_ids_width, tdata.n_stop_points);
        tdata.vjid_index = radixtree_load_strings_from_tdata (tdata.vj_ids, tdata.vj_ids_width, tdata.n_vjs);
        tdata.lineid_index = radixtree_load_strings_from_tdata (tdata.line_ids, tdata.line_ids_width, tdata.n_journey_patterns);

        /* Validate the radixtrees are actually created. */
        if (!(tdata.stop_point_id_index &&
              tdata.vjid_index &&
              tdata.lineid_index)) {
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

plan:
    /* While the scratch space remains allocated, each new search may require
     * reinitialisation of this memory.
     */
    router_reset (&router);

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

    if ( ! router_route (&router, &req)) {
        /* if the search failed we must exit */
        status = EXIT_FAILURE;
        goto clean_exit;
    }

    /* To debug the router, we render an intermediate result */
    if (cli_args.verbose) {
        char result_buf[OUTPUT_LEN];
        router_request_dump (&req, &tdata);
        router_result_dump(&router, &req, result_buf, OUTPUT_LEN);
        puts(result_buf);
    }

    /* When searching clockwise we will board any vehicle_journey that will bring us at
     * the earliest time at any destination location. If we have to wait at
     * some stage for a connection, and this wait time exceeds the frequency
     * of the ingress network, we may suggest a later departure decreases
     * overal waitingtime.
     *
     * To compress waitingtime we employ a reversal. A clockwise search
     * departing at 9:00am and arriving at 10:00am is observed as was
     * requested: what vehicle_journey allows to arrive at 10:00am? The counter clockwise
     * search starts at 10:00am and offers the last possible arrival at 9:15am.
     * This bounds our searchspace between 9:15am and 10:00am.
     *
     * Because of the memory structure. We are not able to render an arrive-by
     * search, therefore the second arrival will start at 9:15am and should
     * render exactly the same vehicle_journey. This is not always true, especially not
     * when there are multiple paths with exactly the same transittime.
     *
     *
     * For an arrive_by counter clockwise search, we must make the result
     * clockwise. Only one reversal is required. For the more regular clockwise
     * search, the compression is handled in the first reversal (ccw) and made
     * clockwise in the second reversal.
     */

    {
        uint8_t i;
        uint8_t n_reversals = req.arrive_by ? 1 : 2;
        for (i = 0; i < n_reversals; ++i) {
            if ( ! router_request_reverse (&router, &req)) {
                /* if the reversal fails we must exit */
                status = EXIT_FAILURE;
                goto clean_exit;
            }

            router_reset (&router);

            if ( ! router_route (&router, &req)) {
                status = EXIT_FAILURE;
                goto clean_exit;
            }

            if (cli_args.verbose) {
                char result_buf[OUTPUT_LEN];
                puts ("Repeated search with reversed request: \n");
                router_request_dump (&req, &tdata);
                router_result_dump (&router, &req, result_buf, OUTPUT_LEN);
                puts (result_buf);
            }
        }
    }

    /* * * * * * * * * * * * * * * * * * *
     *  PHASE THREE: RENDER THE RESULTS
     *
     * * * * * * * * * * * * * * * * * * */

    /* Output only final result in non-verbose mode */
    if ( ! cli_args.verbose) {
        char result_buf[OUTPUT_LEN] = "";
        router_result_dump(&router, &req, result_buf, OUTPUT_LEN);

        /* For benchmarking: repeat the search up to n time */
        if (cli_args.repeat > 0) {
            cli_args.repeat--;
            goto plan;
        }

        puts (result_buf);
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

    #ifdef RRRR_FEATURE_REALTIME
    if (tdata.stop_point_id_index) radixtree_destroy (tdata.stop_point_id_index);
    if (tdata.vjid_index) radixtree_destroy (tdata.vjid_index);
    if (tdata.lineid_index) radixtree_destroy (tdata.lineid_index);
    #endif

    /* Unmap the memory and/or deallocate the memory on the heap */
    tdata_close (&tdata);

    #ifdef RRRR_VALGRIND
    goto fast_exit; /* kills the unused label warning */
    #endif

fast_exit:
    exit(status);
}
