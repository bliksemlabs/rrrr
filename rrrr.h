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

#define PROGRAM_NAME "rrrr"

typedef enum {FALSE = 0, TRUE} boolean;

typedef struct {
    char version_string[8];
    int nstops;
    int nroutes;
    int loc_stops;
    int loc_routes;
    int loc_route_stops;
    int loc_stop_times;
    int loc_stop_routes;
    int loc_transfers; 
} timetable_header_t;

typedef struct {
    float lat;
    float lon;
} stop_coord_t;

typedef struct {
    int stop_routes_offset;
    int transfers_offset;
} stop_t;

typedef struct {
    int route_stops_offset;
    int stop_times_offset;
} route_t;

typedef struct {
    int target_stop;
    float dist_meters;
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
    
    stop_coord_t *stop_coords;
    stop_t *stops;
    route_t *routes;
    int *route_stops;
    int *stop_times;
    int *stop_routes;
    xfer_t *transfers; 
    
};

boolean parse_query_params(request_t *req);

void url_decode (char *buf);

#endif // CRAPTOR_H
