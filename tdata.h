/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* tdata.h */
#ifndef _TDATA_H
#define _TDATA_H

#include "geometry.h"
#include "util.h"
#include "radixtree.h"
#include "gtfs-realtime.pb-c.h"

#include <stddef.h>

typedef uint32_t calendar_t;

typedef struct stop stop_t;
struct stop {
    uint32_t stop_routes_offset;
    uint32_t transfers_offset;
};

/* An individual Route in the RAPTOR sense: A group of VehicleJourneys all having the same JourneyPattern. */
typedef struct rrrr_route rrrr_route_t;
struct rrrr_route {
    uint32_t route_stops_offset;
    uint32_t trip_ids_offset;
    uint32_t headsign_offset;
    uint16_t n_stops;
    uint16_t n_trips;
    uint16_t attributes;
    uint16_t agency_index;
    uint16_t shortname_index;
    uint16_t productcategory_index;
    rtime_t  min_time;
    rtime_t  max_time;
};

/* An individual VehicleJourney, a materialized instance of a time demand type. */
typedef struct trip trip_t;
struct trip {
    uint32_t stop_times_offset; // The offset of the first stoptime of the time demand type used by this trip
    rtime_t  begin_time;        // The absolute start time since at the departure of the first stop
    int16_t  realtime_delay;    // This is signed to indicate early or late. All zeros upon creation (but serves as padding).
};

typedef struct stoptime stoptime_t;
struct stoptime {
    rtime_t arrival;
    rtime_t departure;
};

typedef enum stop_attribute {
    sa_wheelchair_boarding  =   1, // wheelchair accessible
    sa_visual_accessible    =   2, // accessible for blind people
    sa_shelter              =   4, // roof against rain
    sa_bikeshed             =   8, // you can put your bike somewhere
    sa_bicyclerent          =  16, // you can rent a bicycle
    sa_parking              =  32  // carparking is available
} stop_attribute_t;

typedef enum routestop_attribute {
    rsa_waitingpoint =   1, // at this stop the vehicle waits if its early
    rsa_boarding     =   2, // a passenger can enter the vehicle at this stop
    rsa_alighting    =   4  // a passenger can leave the vehicle at this stop
} routestop_attribute_t;

// treat entirely as read-only?
typedef struct tdata tdata_t;
struct tdata {
    void *base;
    size_t size;
    // required data
    uint64_t calendar_start_time; // midnight of the first day in the 32-day calendar in seconds since the epoch, DST ignorant
    calendar_t dst_active;
    uint32_t n_stops;
    uint32_t n_routes;
    uint32_t n_trips;
    uint32_t n_agencies;
    stop_t *stops;
    uint8_t *stop_attributes;
    rrrr_route_t *routes;
    uint32_t *route_stops;
    uint8_t  *route_stop_attributes;
    stoptime_t *stop_times;
    trip_t *trips;
    uint32_t *stop_routes;
    uint32_t *transfer_target_stops;
    uint8_t  *transfer_dist_meters;
    // optional data -- NULL pointer means it is not available
    latlon_t *stop_coords;
    uint32_t platformcode_width;
    char *platformcodes;
    char *stop_names;
    uint32_t *stop_nameidx;
    uint32_t agency_id_width;
    char *agency_ids;
    uint32_t agency_name_width;
    char *agency_names;
    uint32_t agency_url_width;
    char *agency_urls;
    char *headsigns;
    uint32_t route_shortname_width;
    char *route_shortnames;
    uint32_t productcategory_width;
    char *productcategories;
    calendar_t *trip_active;
    calendar_t *route_active;
    uint8_t *trip_attributes;
    uint32_t route_id_width;
    char *route_ids;
    uint32_t stop_id_width;
    char *stop_ids;
    uint32_t trip_id_width;
    char *trip_ids;
    TransitRealtime__FeedMessage *alerts;
};

void tdata_load(char* filename, tdata_t*);

void tdata_close(tdata_t*);

void tdata_dump(tdata_t*);

uint32_t *tdata_stops_for_route(tdata_t *, uint32_t route);

uint8_t *tdata_stop_attributes_for_route(tdata_t *, uint32_t route);

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
uint32_t tdata_routes_for_stop(tdata_t*, uint32_t stop, uint32_t **routes_ret);

stoptime_t *tdata_stoptimes_for_route(tdata_t*, uint32_t route_index);

void tdata_dump_route(tdata_t*, uint32_t route_index, uint32_t trip_index);

char *tdata_route_id_for_index(tdata_t*, uint32_t route_index);

char *tdata_stop_id_for_index(tdata_t*, uint32_t stop_index);

uint8_t *tdata_stop_attributes_for_index(tdata_t*, uint32_t stop_index);

char *tdata_trip_id_for_index(tdata_t*, uint32_t trip_index);

char *tdata_trip_id_for_route_trip_index(tdata_t *td, uint32_t route_index, uint32_t trip_index);

uint32_t tdata_agencyidx_by_agency_name(tdata_t*, char* agency_name, uint32_t start_index);

char *tdata_agency_id_for_index(tdata_t *td, uint32_t agency_index);

char *tdata_agency_name_for_index(tdata_t *td, uint32_t agency_index);

char *tdata_agency_url_for_index(tdata_t *td, uint32_t agency_index);

char *tdata_headsign_for_offset(tdata_t *td, uint32_t headsign_offset);

char *tdata_route_shortname_for_index(tdata_t *td, uint32_t route_shortname_index);

char *tdata_productcategory_for_index(tdata_t *td, uint32_t productcategory_index);

char *tdata_stop_name_for_index(tdata_t*, uint32_t stop_index);

char *tdata_platformcode_for_index(tdata_t*, uint32_t stop_index);

uint32_t tdata_stopidx_by_stop_name(tdata_t*, char* stop_name, uint32_t start_index);

uint32_t tdata_stopidx_by_stop_id(tdata_t*, char* stop_id, uint32_t start_index);

uint32_t tdata_routeidx_by_route_id(tdata_t*, char* route_id, uint32_t start_index);

char *tdata_trip_ids_for_route(tdata_t*, uint32_t route_index);

uint8_t *tdata_trip_attributes_for_route(tdata_t*, uint32_t route_index);

calendar_t *tdata_trip_masks_for_route(tdata_t*, uint32_t route_index);

char *tdata_headsign_for_route(tdata_t*, uint32_t route_index);

char *tdata_shortname_for_route(tdata_t*, uint32_t route_index);

char *tdata_productcategory_for_route(tdata_t*, uint32_t route_index);

char *tdata_agency_id_for_route(tdata_t*, uint32_t route_index);

char *tdata_agency_name_for_route(tdata_t*, uint32_t route_index);

char *tdata_agency_url_for_route(tdata_t*, uint32_t route_index);

/* Returns a pointer to the first stoptime for the trip (VehicleJourney). These are generally TimeDemandTypes that must
   be shifted in time to get the true scheduled arrival and departure times. */
stoptime_t *tdata_timedemand_type(tdata_t*, uint32_t route_index, uint32_t trip_index);

/* Get a pointer to the array of trip structs for this route. */
trip_t *tdata_trips_for_route(tdata_t *td, uint32_t route_index);

void tdata_apply_gtfsrt (tdata_t *tdata, RadixTree *tripid_index, uint8_t *buf, size_t len);

void tdata_apply_gtfsrt_file (tdata_t *tdata, RadixTree *tripid_index, char *filename);

void tdata_clear_gtfsrt (tdata_t *tdata);

void tdata_apply_gtfsrt_alerts (tdata_t *tdata, RadixTree *routeid_index, RadixTree *stopid_index, RadixTree *tripid_index, uint8_t *buf, size_t len);

void tdata_apply_gtfsrt_alerts_file (tdata_t *tdata, RadixTree *routeid_index, RadixTree *stopid_index, RadixTree *tripid_index, char *filename);

void tdata_clear_gtfsrt_alerts (tdata_t *tdata);

/* The signed delay of the specified trip in seconds. */
float tdata_delay_min (tdata_t *td, uint32_t route_index, uint32_t trip_index);

#endif // _TDATA_H

