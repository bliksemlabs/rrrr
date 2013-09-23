/* worker.c : worker processes that handle routing requests */

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "rrrr.h"
#include "tdata.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage:\n%s timetable.dat ROUTE route_idx [trip_idx]\n"
               "                            ROUTEID route_id\n"
               "                            STOP stop_idx\n"
               "                            STOPID stop_id\n"
               "                            STOPNAME stopname\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    /* SETUP */
    
    // load transit data from disk
    tdata_t tdata;
    tdata_load(argv[1], &tdata);
    // tdata_dump(&tdata);

    if (strcmp(argv[2], "ROUTE") == 0) {
        if (argc > 4) {
            tdata_dump_route(&tdata, strtol(argv[3], NULL, 10), strtol(argv[4], NULL, 10));
        } else {
            tdata_dump_route(&tdata, strtol(argv[3], NULL, 10), NONE);
        }
    } else if (strcmp(argv[2], "ROUTEID") == 0) {
        uint32_t route_index = tdata_routeidx_by_route_id(&tdata, argv[3], 0);
        while (route_index != NONE || route_index < tdata.n_stops) {
            tdata_dump_route(&tdata, route_index, NONE);
            route_index = tdata_routeidx_by_route_id(&tdata, argv[3], route_index + 1);
        }
    } else if (argv[2][0] == 'S') {
        if (strcmp(argv[2], "STOP") == 0) {
            uint32_t stop_index = strtol(argv[3], NULL, 10);
            if (stop_index > tdata.n_stops) {
                fprintf(stderr, "Only %d stops in %s\n", tdata.n_stops, argv[1]);
            } else {
                printf ("%d %s %s\n", stop_index, tdata_stop_id_for_index(&tdata, stop_index), tdata_stop_desc_for_index(&tdata, stop_index));
            }
        } else if (strcmp(argv[2], "STOPID") == 0) {
            uint32_t stop_index = tdata_stopidx_by_stop_id(&tdata, argv[3], 0);
            if (stop_index <= tdata.n_stops) {
                printf ("%d %s %s\n", stop_index, tdata_stop_id_for_index(&tdata, stop_index), tdata_stop_desc_for_index(&tdata, stop_index));
            }
        } else if (strcmp(argv[2], "STOPNAME") == 0) {
            uint32_t stop_index = tdata_stopidx_by_stop_desc(&tdata, argv[3], 0);
            while (stop_index != NONE || stop_index < tdata.n_stops) {
                printf ("%d %s %s\n", stop_index, tdata_stop_id_for_index(&tdata, stop_index), tdata_stop_desc_for_index(&tdata, stop_index));
                stop_index = tdata_stopidx_by_stop_desc(&tdata, argv[3], stop_index + 1);
            }
        }
    }

    tdata_close(&tdata);
    exit(EXIT_SUCCESS);
}

