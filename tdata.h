/* tdata.h */
#ifndef _TDATA_H
#define _TDATA_H

#include "geometry.h"
#include "util.h"
#include <stddef.h>

// hide these details?
typedef struct stop stop_t;
struct stop {
    int stop_routes_offset;
    int transfers_offset;
};

typedef struct route route_t;
struct route { // switch to unsigned
    int route_stops_offset;
    int stop_times_offset;
    int trip_ids_offset;
    int n_stops;
    int n_trips;
};

typedef struct transfer transfer_t;
struct transfer {
    int target_stop;
    float dist_meters;
};

typedef struct stoptime stoptime_t;
struct stoptime {
    rtime_t arrival;
    rtime_t departure;
};

// treat entirely as read-only?
typedef struct tdata tdata_t;
struct tdata {
    void *base;
    size_t size;
    // required data
    int n_stops;
    int n_routes;
    stop_t *stops;
    route_t *routes;
    int *route_stops;
    stoptime_t *stop_times;
    int *stop_routes;
    transfer_t *transfers; 
    // optional data -- NULL pointer means it is not available
    coord_t *stop_coords;
    int stop_id_width;
    char *stop_ids;
    int route_id_width;
    char *route_ids;
    int trip_id_width;
    char *trip_ids;
    uint32_t *trip_active;
    uint32_t *route_active;
};

void tdata_load(char* /*filename*/, tdata_t*);

void tdata_close(tdata_t*);

void tdata_dump(tdata_t*);

int *tdata_stops_for_route(tdata_t, int route);

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
int tdata_routes_for_stop(tdata_t*, int stop, int **routes_ret);

stoptime_t *tdata_stoptimes_for_route(tdata_t*, int route_index);

void tdata_dump_route(tdata_t*, int route_index);

char *tdata_stop_id_for_index(tdata_t*, int stop_index);

char *tdata_route_id_for_index(tdata_t*, int route_index);

char *tdata_trip_ids_for_route(tdata_t*, int route_index);

uint32_t *tdata_trip_masks_for_route(tdata_t*, int route_index);

#endif // _TDATA_H

