/* worker.c : worker processes that handle routing requests */

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

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage:\n%s timetable.dat ROUTE route_idx [trip_idx]\n", argv[0]);
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
    }

    tdata_close(&tdata);
    exit(EXIT_SUCCESS);
}

