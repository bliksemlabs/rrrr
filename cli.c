/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/ */

/* cli.c : single-threaded commandline interface to the library */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "stubs.h"
#include "router_request.h"

#define OUTPUT_LEN 8000

typedef struct cli_arguments cli_arguments_t;
struct cli_arguments {
    char *gtfsrt_filename;
    bool verbose;
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


    /* * * * * * * * * * * * * * * * * * * * * *
     * PHASE ZERO: HANDLE COMMANDLINE ARGUMENTS
     *
     * * * * * * * * * * * * * * * * * * * * * */

    if (argc < 3) {
        fprintf(stderr, "Usage:\n%s timetable.dat\n" \
                        "[ --verbose ] [ --randomize ]\n" \
                        "[ --arrive=YYYY-MM-DDTHH:MM:SS | --depart=YYYY-MM-DDTHH:MM:SS ]\n" \
                        "[ --from-idx=idx | --from-latlon=Y,X ]\n" \
                        "[ --via-idx=idx  | --via-latlon=Y,X ]\n" \
                        "[ --to-idx=idx   | --to-latlon=Y,X ]\n" \
                        "[ --gtfsrt=filename.pb ]\n"
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

    /* initialise the cli settings */
    cli_args.verbose = false;
    cli_args.gtfsrt_filename = NULL;

    {
        int i;
        for (i = 1; i < (argc - 1); i++) {
            if (argv[i + 0][0] == '-' && argv[i + 0][1] == '-') {
                /* all arguments will be in the form --argument= */

                switch (argv[i][2]) {
                case 'a':
                    if (strncmp(argv[i], "--arrive=", 9) == 0) {
                        router_request_from_epoch (&req, &tdata, strtoepoch(&argv[i][9]));
                        req.arrive_by = true;
                    }
                    break;

                case 'd':
                    if (strncmp(argv[i], "--depart=", 9) == 0) {
                        router_request_from_epoch (&req, &tdata, strtoepoch(&argv[i][9]));
                        req.arrive_by = false;
                    }
                    break;

                case 'f':
                    if (strncmp(argv[i], "--from-idx=", 11) == 0) {
                        req.from = (uint32_t) strtol(&argv[i][11], NULL, 10);
                    } else
                    if (strncmp(argv[i], "--from-latlon=", 14) == 0) {
                        strtolatlon(&argv[i][12], &req.from_latlon);
                    }
                    break;

                case 'g':
                    if (strncmp(argv[i], "--gtfsrt=", 9) == 0) {
                        cli_args.gtfsrt_filename = &argv[i][9];
                    }
                    break;

                case 'r':
                    if (strcmp(argv[i], "--randomize") == 0) {
                    }
                    break;

                case 't':
                    if (strncmp(argv[i], "--to-idx=", 9) == 0) {
                        req.to = (uint32_t) strtol(&argv[i][9], NULL, 10);
                    } else
                    if (strncmp(argv[i], "--to-latlon=", 12) == 0) {
                        strtolatlon(&argv[i][12], &req.to_latlon);
                    }
                    break;

                case 'v':
                    if (strcmp(argv[i], "--verbose") == 0) {
                        cli_args.verbose = true;
                    } else
                    if (strncmp(argv[i], "--via=", 6) == 0) {
                        req.via = (uint32_t) strtol(&argv[i][6], NULL, 10);
                    } else
                    if (strncmp(argv[i], "--via-latlon=", 13) == 0) {
                        strtolatlon(&argv[i][13], &req.via_latlon);
                    }
                    break;

                case 'w':
                    if (strncmp(argv[i], "--walk-speed=", 13) == 0) {
                        req.walk_speed = (float) strtod(&argv[i][13], NULL);
                    } else
                    if (strncmp(argv[i], "--walk-slack=", 13) == 0) {
                        req.walk_slack = (float) strtod(&argv[i][13], NULL);
                    }
                    break;
                }
            }
        }
    }

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

    /* While the scratch space remains allocated, each new search may require
     * reinitialisation of this memory.
     */
    router_reset (&router);

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
        router_request_dump (&router, &req);
        router_result_dump(&router, &req, result_buf, OUTPUT_LEN);
        puts(result_buf);
    }

    /* When searching clockwise we will board any trip that will bring us at
     * the earliest time at any destination location. If we have to wait at
     * some stage for a connection, and this wait time exceeds the frequency
     * of the ingress network, we may suggest a later departure decreases
     * overal waitingtime.
     *
     * To compress waitingtime we employ a reversal. A clockwise search
     * departing at 9:00am and arriving at 10:00am is observed as was
     * requested: what trip allows to arrive at 10:00am? The counter clockwise
     * search starts at 10:00am and offers the last possible arrival at 9:15am.
     * This bounds our searchspace between 9:15am and 10:00am.
     *
     * Because of the memory structure. We are not able to render an arrive-by
     * search, therefore the second arrival will start at 9:15am and should
     * render exactly the same trip. This is not always true, especially not
     * when there are multiple paths with exactly the same transittime.
     *
     *
     * For an arrive_by counter clockwise search, we must make the result
     * clockwise. Only one reversal is required. For the more regular clockwise
     * search, the compression is handled in the first reversal (ccw) and made
     * clockwise in the second reversal.
     */

    {
        uint32_t i;
        uint32_t n_reversals = req.arrive_by ? 1 : 2;
        for (i = 0; i < n_reversals; ++i) {
            if ( ! router_request_reverse (&router, &req)) {
                /* if the reversal fails we must exit */
                goto clean_exit;
            }
            if (cli_args.verbose) {
                char result_buf[OUTPUT_LEN];
                puts ("Repeated search with reversed request: \n");
                router_request_dump (&router, &req);
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
        char result_buf[OUTPUT_LEN];
        router_result_dump(&router, &req, result_buf, OUTPUT_LEN);
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
    #ifndef DEBUG
    goto fast_exit;
    #endif

    /* Unmap the memory and/or deallocate the memory on the heap */
    tdata_close (&tdata);

    /* Deallocate the scratchspace of the router */
    router_teardown (&router);

fast_exit:
    exit(status);
}
