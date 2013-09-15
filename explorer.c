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
        printf("Usage:\n%stimetable.dat ROUTE route_idx [trip_idx]\n"
               "                           STOP stop_id\n"
               "                           NAME stopname\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    /* SETUP */
    
    // load transit data from disk
    tdata_t tdata;
    tdata_load(argv[1], &tdata);
    // tdata_dump(&tdata);

    if (argv[2][0] == 'R') {
        if (argc > 4) {
            tdata_dump_route(&tdata, strtol(argv[3], NULL, 10), strtol(argv[4], NULL, 10));
        } else {
            tdata_dump_route(&tdata, strtol(argv[3], NULL, 10), NONE);
        }
    } else if (argv[2][0] == 'S') {
        uint32_t stop_index = strtol(argv[3], NULL, 10);
        if (stop_index > tdata.n_stops) {
            fprintf(stderr, "Only %d stops in %s\n", tdata.n_stops, argv[1]);
        } else {
            printf ("%d %s\n", stop_index, tdata_stop_desc_for_index(&tdata, stop_index));
        }
    } else if (argv[2][0] == 'N') {
        uint32_t stop_index = tdata_stop_name_for_index(&tdata, argv[3], 0);
        while (stop_index != NONE || stop_index < tdata.n_stops) {
            printf ("%d %s\n", stop_index, tdata_stop_desc_for_index(&tdata, stop_index));
            stop_index = tdata_stop_name_for_index(&tdata, argv[3], stop_index + 1);
        }
    }

    tdata_close(&tdata);
    exit(EXIT_SUCCESS);
}

