/* transitdata.h */
#ifndef _TRANSIT_DATA_H
#define _TRANSIT_DATA_H

#include "geometry.h"
#include <stddef.h>

// hide these details?
typedef struct stop stop_t;
struct stop {
    int stop_routes_offset;
    int transfers_offset;
};

typedef struct route route_t;
struct route {
    int route_stops_offset;
    int stop_times_offset;
    int trip_ids_offset;
};

typedef struct transfer transfer_t;
struct transfer {
    int target_stop;
    float dist_meters;
};

typedef struct transit_data transit_data_t;
struct transit_data {
    void *base;
    size_t size;
    // required data
    int nstops;
    int nroutes;
    stop_t *stops;
    route_t *routes;
    int *route_stops;
    int *stop_times;
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
};

void transit_data_load(char* /*filename*/, transit_data_t*);

void transit_data_close(transit_data_t*);

void transit_data_dump(transit_data_t*);


int *transit_data_stops_for_route(transit_data_t, int route, int **next_route);

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
int transit_data_routes_for_stop(transit_data_t*, int stop, int **routes_ret);

int *transit_data_stoptimes_for_route(transit_data_t, int route, int **next_route);

void transit_data_dump_route(transit_data_t *td, int route);

char *transit_data_stop_id_for_index(transit_data_t*, int stop_index);

char *transit_data_route_id_for_index(transit_data_t*, int route_index);

char *transit_data_trip_ids_for_route_index(transit_data_t*, int route_index);

#endif // _TRANSIT_DATA_H

