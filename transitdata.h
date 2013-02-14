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
};

void transit_data_load(char* /*filename*/, transit_data_t*);

void transit_data_close(transit_data_t*);

void transit_data_dump(transit_data_t*);

int *transit_data_stops_for_route(transit_data_t, int route, int **next_route);

int *transit_data_stoptimes_for_route(transit_data_t, int route, int **next_route);

void transit_data_dump_route(transit_data_t *td, int route);

char *transit_data_stop_id_for_index(transit_data_t*, int index);

#endif // _TRANSIT_DATA_H

