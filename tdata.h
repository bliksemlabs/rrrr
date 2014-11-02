/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* tdata.h */

#ifndef _TDATA_H
#define _TDATA_H

#include "config.h"
#include "geometry.h"
#include "rrrr_types.h"

#ifdef RRRR_FEATURE_REALTIME
#include "gtfs-realtime.pb-c.h"
#include "radixtree.h"
#endif

#include <stddef.h>
#include <stdbool.h>

typedef struct stop stop_t;
struct stop {
    uint32_t stop_routes_offset;
    uint32_t transfers_offset;
};

/* An individual Route in the RAPTOR sense:
 * A group of VehicleJourneys all share the same JourneyPattern.
 */
typedef struct route route_t;
struct route {
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

/* An individual VehicleJourney,
 * a materialized instance of a time demand type. */
typedef struct trip trip_t;
struct trip {
    /* The offset of the first stoptime of the
     * time demand type used by this trip.
     */
    uint32_t stop_times_offset;

    /* The absolute start time since at the
     * departure of the first stop
     */
    rtime_t  begin_time;

    /* The trip_attributes, including CANCELED flag
     */
    uint16_t trip_attributes;
};

typedef struct stoptime stoptime_t;
struct stoptime {
    rtime_t arrival;
    rtime_t departure;
};

typedef enum stop_attribute {
    /* the stop is accessible for a wheelchair */
    sa_wheelchair_boarding  =   1,

    /* the stop is accessible for the visible impaired */
    sa_visual_accessible    =   2,

    /* a shelter is available against rain */
    sa_shelter              =   4,

    /* a bicycle can be parked */
    sa_bikeshed             =   8,

    /* a bicycle may be rented */
    sa_bicyclerent          =  16,

    /* a car can be parked */
    sa_parking              =  32
} stop_attribute_t;

typedef enum routestop_attribute {
    /* the vehicle waits if it arrives early */
    rsa_waitingpoint =   1,

    /* a passenger can enter the vehicle at this stop */
    rsa_boarding     =   2,

    /* a passenger can leave the vehicle at this stop */
    rsa_alighting    =   4
} routestop_attribute_t;

typedef struct tdata tdata_t;
struct tdata {
    void *base;
    size_t size;
    /* Midnight of the first day in the 32-day calendar in seconds
     * since the epoch, ignores Daylight Saving Time (DST).
     */
    uint64_t calendar_start_time;

    /* Dates within the active calendar which have DST. */
    calendar_t dst_active;
    uint32_t n_stops;
    uint32_t n_stop_attributes;
    uint32_t n_stop_coords;
    uint32_t n_routes;
    uint32_t n_route_stops;
    uint32_t n_route_stop_attributes;
    uint32_t n_stop_times;
    uint32_t n_trips;
    uint32_t n_stop_routes;
    uint32_t n_transfer_target_stops;
    uint32_t n_transfer_dist_meters;
    uint32_t n_trip_active;
    uint32_t n_route_active;
    uint32_t n_platformcodes;
    uint32_t n_stop_names;
    uint32_t n_stop_nameidx;
    uint32_t n_agency_ids;
    uint32_t n_agency_names;
    uint32_t n_agency_urls;
    uint32_t n_headsigns;
    uint32_t n_route_shortnames;
    uint32_t n_productcategories;
    uint32_t n_route_ids;
    uint32_t n_stop_ids;
    uint32_t n_trip_ids;
    stop_t *stops;
    uint8_t *stop_attributes;
    route_t *routes;
    uint32_t *route_stops;
    uint8_t  *route_stop_attributes;
    stoptime_t *stop_times;
    trip_t *trips;
    uint32_t *stop_routes;
    uint32_t *transfer_target_stops;
    uint8_t  *transfer_dist_meters;
    /* optional data:
     * NULL pointer means it is not available */
    latlon_t *stop_coords;
    uint32_t platformcodes_width;
    char *platformcodes;
    char *stop_names;
    uint32_t *stop_nameidx;
    uint32_t agency_ids_width;
    char *agency_ids;
    uint32_t agency_names_width;
    char *agency_names;
    uint32_t agency_urls_width;
    char *agency_urls;
    char *headsigns;
    uint32_t route_shortnames_width;
    char *route_shortnames;
    uint32_t productcategories_width;
    char *productcategories;
    calendar_t *trip_active;
    calendar_t *route_active;
    uint32_t route_ids_width;
    char *route_ids;
    uint32_t stop_ids_width;
    char *stop_ids;
    uint32_t trip_ids_width;
    char *trip_ids;
    #ifdef RRRR_FEATURE_REALTIME
    RadixTree *routeid_index;
    RadixTree *stopid_index;
    RadixTree *tripid_index;
    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    stoptime_t **trip_stoptimes;
    uint32_t *trip_routes;
    list_t **rt_stop_routes;
    #endif
    #ifdef RRRR_FEATURE_REALTIME_ALERTS
    TransitRealtime__FeedMessage *alerts;
    #endif
    #endif
};

bool tdata_load(tdata_t *td, char *filename);

void tdata_close(tdata_t *td);

void tdata_dump(tdata_t *td);

uint32_t *tdata_stops_for_route(tdata_t *td, uint32_t route);

uint8_t *tdata_stop_attributes_for_route(tdata_t *td, uint32_t route);

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
uint32_t tdata_routes_for_stop(tdata_t *td, uint32_t stop, uint32_t **routes_ret);

stoptime_t *tdata_stoptimes_for_route(tdata_t *td, uint32_t route_index);

void tdata_dump_route(tdata_t *td, uint32_t route_index, uint32_t trip_index);

char *tdata_route_id_for_index(tdata_t *td, uint32_t route_index);

char *tdata_stop_id_for_index(tdata_t *td, uint32_t stop_index);

uint8_t *tdata_stop_attributes_for_index(tdata_t *td, uint32_t stop_index);

char *tdata_trip_id_for_index(tdata_t *td, uint32_t trip_index);

char *tdata_trip_id_for_route_trip_index(tdata_t *td, uint32_t route_index, uint32_t trip_index);

uint32_t tdata_agencyidx_by_agency_name(tdata_t *td, char* agency_name, uint32_t start_index);

char *tdata_agency_id_for_index(tdata_t *td, uint32_t agency_index);

char *tdata_agency_name_for_index(tdata_t *td, uint32_t agency_index);

char *tdata_agency_url_for_index(tdata_t *td, uint32_t agency_index);

char *tdata_headsign_for_offset(tdata_t *td, uint32_t headsign_offset);

char *tdata_route_shortname_for_index(tdata_t *td, uint32_t route_shortname_index);

char *tdata_productcategory_for_index(tdata_t *td, uint32_t productcategory_index);

char *tdata_stop_name_for_index(tdata_t *td, uint32_t stop_index);

char *tdata_platformcode_for_index(tdata_t *td, uint32_t stop_index);

uint32_t tdata_stopidx_by_stop_name(tdata_t *td, char* stop_name, uint32_t start_index);

uint32_t tdata_stopidx_by_stop_id(tdata_t *td, char* stop_id, uint32_t start_index);

uint32_t tdata_routeidx_by_route_id(tdata_t *td, char* route_id, uint32_t start_index);

char *tdata_trip_ids_for_route(tdata_t *td, uint32_t route_index);

calendar_t *tdata_trip_masks_for_route(tdata_t *td, uint32_t route_index);

char *tdata_headsign_for_route(tdata_t *td, uint32_t route_index);

char *tdata_shortname_for_route(tdata_t *td, uint32_t route_index);

char *tdata_productcategory_for_route(tdata_t *td, uint32_t route_index);

char *tdata_agency_id_for_route(tdata_t *td, uint32_t route_index);

char *tdata_agency_name_for_route(tdata_t *td, uint32_t route_index);

char *tdata_agency_url_for_route(tdata_t *td, uint32_t route_index);

/* Returns a pointer to the first stoptime for the trip (VehicleJourney). These are generally TimeDemandTypes that must
   be shifted in time to get the true scheduled arrival and departure times. */
stoptime_t *tdata_timedemand_type(tdata_t *td, uint32_t route_index, uint32_t trip_index);

/* Get a pointer to the array of trip structs for this route. */
trip_t *tdata_trips_for_route(tdata_t *td, uint32_t route_index);

char *tdata_stop_desc_for_index(tdata_t *td, uint32_t stop_index);

rtime_t transfer_duration (tdata_t *tdata, router_request_t *req, uint32_t stop_index_from, uint32_t stop_index_to);

uint32_t transfer_distance (tdata_t *tdata, uint32_t stop_index_from, uint32_t stop_index_to);

char *tdata_stop_name_for_index(tdata_t *td, uint32_t stop_index);

#endif /* _TDATA_H */
