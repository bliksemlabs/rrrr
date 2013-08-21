/* tdata.h */
#ifndef _TDATA_H
#define _TDATA_H

#include "geometry.h"
#include "util.h"
#include <stddef.h>

typedef struct stop stop_t;
struct stop {
    uint32_t stop_routes_offset;
    uint32_t transfers_offset;
};

typedef struct route route_t;
struct route {
    uint32_t route_stops_offset;
    uint32_t stop_times_offset;
    uint32_t trip_ids_offset;
    uint32_t n_stops;
    uint32_t n_trips;
    rtime_t  first;
    rtime_t  last;
};

/* An individual VehicleJourney, a materialized instance of a time demand type. */
typedef struct trip trip_t;
struct trip {
    uint32_t stop_times_offset; // The offset of the first stoptime of the time demand type used by this trip
    uint32_t first_departure;   // This could be rtime_t but struct will be 64 bits anyway due to padding.
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
    uint64_t calendar_start_time; // midnight of the first day in the 32-day calendar in seconds since the epoch
    uint32_t n_stops;
    uint32_t n_routes;
    stop_t *stops;
    route_t *routes;
    uint32_t *route_stops;
    stoptime_t *stop_times;
    trip_t *trips;
    uint32_t *stop_routes;
    uint32_t *transfer_target_stops;
    uint8_t  *transfer_dist_meters;
    // optional data -- NULL pointer means it is not available
    latlon_t *stop_coords;
    uint32_t stop_id_width;
    char *stop_ids;
    uint32_t route_id_width;
    char *route_ids;
    uint32_t trip_id_width;
    char *trip_ids;
    uint32_t *trip_active;
    uint32_t *route_active;
};

void tdata_load(char* filename, tdata_t*);

void tdata_close(tdata_t*);

void tdata_dump(tdata_t*);

uint32_t *tdata_stops_for_route(tdata_t, uint32_t route);

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
uint32_t tdata_routes_for_stop(tdata_t*, uint32_t stop, uint32_t **routes_ret);

stoptime_t *tdata_stoptimes_for_route(tdata_t*, uint32_t route_index);

void tdata_dump_route(tdata_t*, uint32_t route_index, uint32_t trip_index);

char *tdata_stop_id_for_index(tdata_t*, uint32_t stop_index);

uint32_t tdata_stop_name_for_index(tdata_t*, char* stop_name, uint32_t start_index);

char *tdata_route_id_for_index(tdata_t*, uint32_t route_index);

char *tdata_trip_ids_for_route(tdata_t*, uint32_t route_index);

uint32_t *tdata_trip_masks_for_route(tdata_t*, uint32_t route_index);

/* Returns a pointer to the first stoptime for the trip (VehicleJourney). These are generally TimeDemandTypes that must 
   be shifted in time to get the true scheduled arrival and departure times. */
stoptime_t *tdata_timedemand_type(tdata_t*, uint32_t route_index, uint32_t trip_index);

/* Get a pointer to the array of trip structs for this route. */
trip_t *tdata_trips_for_route(tdata_t *td, uint32_t route_index);

/* Get one specific departure time for the given route, trip, and stop (stop number within the trip, not global stop index) */ 
rtime_t tdata_depart(tdata_t *td, uint32_t route_index, uint32_t trip_index, uint32_t stop_index);

/* Get one specific arrival time for the given route, trip, and stop (stop number within the trip, not global stop index) */ 
rtime_t tdata_arrive(tdata_t *td, uint32_t route_index, uint32_t trip_index, uint32_t stop_index);
 
#endif // _TDATA_H

