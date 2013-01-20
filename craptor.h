#ifndef CRAPTOR_H
#define CRAPTOR_H

#include "fcgi_stdio.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define PROGRAM_NAME "craptor"

typedef enum {FALSE = 0, TRUE} boolean;

typedef struct {
    float lat;
    float lon;
} stop_t;

typedef struct {
    int stops_offset;
    int stop_times_offset;
} route_t;

typedef struct {
    int target_stop;
    float dist_m;
} xfer_t;

typedef struct {
    int from;
    int to;
    long time;
    double walk_speed;
    boolean arrive_by; 
} request_t;

struct context {
    void *data;
    size_t data_size;

    int nstops;
    int nroutes;
    
    stop_t *stops;
    int *stop_routes_offsets;

    int *route_stops_offsets; // per route
    int *stop_times_offsets;  // per route (each trip is of the same size)
    int *route_stops; // for each route, the stops visited in order
    int *dep_times; // for each trip on each route, the departure times    
    int *arv_times; // for each trip on each route, the arrival times    

    xfer_t *transfers;
};

boolean parse_query_params(request_t *req);

void url_decode (char *buf);

#endif // CRAPTOR_H
