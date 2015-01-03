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
    uint32_t journey_patterns_at_stop_offset;
    uint32_t transfers_offset;
};

/* An individual JourneyPattern in the RAPTOR sense:
 * A group of VehicleJourneys all share the same JourneyPattern.
 */
typedef struct journey_pattern journey_pattern_t;
struct journey_pattern {
    uint32_t journey_pattern_point_offset;
    uint32_t vj_ids_offset;
    uint32_t headsign_offset;
    uint16_t n_stops;
    uint16_t n_vjs;
    uint16_t attributes;
    uint16_t agency_index;
    uint16_t line_code_index;
    uint16_t productcategory_index;
    rtime_t  min_time;
    rtime_t  max_time;
};

/* An individual VehicleJourney,
 * a materialized instance of a time demand type. */
typedef struct vehicle_journey vehicle_journey_t;
struct vehicle_journey {
    /* The offset of the first stoptime of the
     * time demand type used by this vehicle_journey.
     */
    uint32_t stop_times_offset;

    /* The absolute start time since at the
     * departure of the first stop
     */
    rtime_t  begin_time;

    /* The vj_attributes, including CANCELED flag
     */
    uint16_t vj_attributes;
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

typedef enum journey_pattern_point_attribute {
    /* the vehicle waits if it arrives early */
    rsa_waitingpoint =   1,

    /* a passenger can enter the vehicle at this stop */
    rsa_boarding     =   2,

    /* a passenger can leave the vehicle at this stop */
    rsa_alighting    =   4
} journey_pattern_point_attribute_t;

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
    uint32_t n_journey_patterns;
    uint32_t n_journey_pattern_points;
    uint32_t n_journey_pattern_point_attributes;
    uint32_t n_stop_times;
    uint32_t n_vjs;
    uint32_t n_journey_patterns_at_stop;
    uint32_t n_transfer_target_stops;
    uint32_t n_transfer_dist_meters;
    uint32_t n_vj_active;
    uint32_t n_journey_pattern_active;
    uint32_t n_platformcodes;
    uint32_t n_stop_names;
    uint32_t n_stop_nameidx;
    uint32_t n_agency_ids;
    uint32_t n_agency_names;
    uint32_t n_agency_urls;
    uint32_t n_headsigns;
    uint32_t n_line_codes;
    uint32_t n_productcategories;
    uint32_t n_line_ids;
    uint32_t n_stop_ids;
    uint32_t n_vj_ids;
    stop_t *stops;
    uint8_t *stop_attributes;
    journey_pattern_t *journey_patterns;
    spidx_t *journey_pattern_points;
    uint8_t  *journey_pattern_point_attributes;
    stoptime_t *stop_times;
    vehicle_journey_t *vjs;
    uint32_t *journey_patterns_at_stop;
    spidx_t *transfer_target_stops;
    uint8_t  *transfer_dist_meters;
    rtime_t max_time;
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
    uint32_t line_codes_width;
    char *line_codes;
    uint32_t productcategories_width;
    char *productcategories;
    calendar_t *vj_active;
    calendar_t *journey_pattern_active;
    uint32_t line_ids_width;
    char *line_ids;
    uint32_t stop_ids_width;
    char *stop_ids;
    uint32_t vj_ids_width;
    char *vj_ids;
    #ifdef RRRR_FEATURE_REALTIME
    radixtree_t *lineid_index;
    radixtree_t *stopid_index;
    radixtree_t *vjid_index;
    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    stoptime_t **vj_stoptimes;
    uint32_t *vjs_in_journey_pattern;
    list_t **rt_journey_patterns_at_stop;
    calendar_t *vj_active_orig;
    calendar_t *journey_pattern_active_orig;
    #endif
    #ifdef RRRR_FEATURE_REALTIME_ALERTS
    TransitRealtime__FeedMessage *alerts;
    #endif
    #endif
};

bool tdata_load(tdata_t *td, char *filename);

void tdata_close(tdata_t *td);

void tdata_dump(tdata_t *td);

spidx_t *tdata_points_for_journey_pattern(tdata_t *td, uint32_t jp_index);

uint8_t *tdata_stop_attributes_for_journey_pattern(tdata_t *td, uint32_t jp_index);

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
uint32_t tdata_journey_patterns_for_stop(tdata_t *td, spidx_t stop_index, uint32_t **jp_ret);

stoptime_t *tdata_stoptimes_for_journey_pattern(tdata_t *td, uint32_t jp_index);

void tdata_dump_journey_pattern(tdata_t *td, uint32_t jp_index, uint32_t vj_index);

const char *tdata_line_id_for_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_stop_id_for_index(tdata_t *td, spidx_t stop_index);

uint8_t *tdata_stop_attributes_for_index(tdata_t *td, spidx_t stop_index);

const char *tdata_vehicle_journey_id_for_index(tdata_t *td, uint32_t vj_index);

const char *tdata_vehicle_journey_id_for_jp_vj_index(tdata_t *td, uint32_t jp_index, uint32_t vj_index);

uint32_t tdata_agencyidx_by_agency_name(tdata_t *td, char* agency_name, uint32_t start_index);

const char *tdata_agency_id_for_index(tdata_t *td, uint32_t agency_index);

const char *tdata_agency_name_for_index(tdata_t *td, uint32_t agency_index);

const char *tdata_agency_url_for_index(tdata_t *td, uint32_t agency_index);

const char *tdata_headsign_for_offset(tdata_t *td, uint32_t headsign_offset);

const char *tdata_line_code_for_index(tdata_t *td, uint32_t line_code_index);

const char *tdata_productcategory_for_index(tdata_t *td, uint32_t productcategory_index);

const char *tdata_stop_name_for_index(tdata_t *td, spidx_t stop_index);

const char *tdata_platformcode_for_index(tdata_t *td, spidx_t stop_index);

spidx_t tdata_stopidx_by_stop_name(tdata_t *td, char* stop_name, spidx_t start_index);

spidx_t tdata_stopidx_by_stop_id(tdata_t *td, char* stop_id, spidx_t start_index);

uint32_t tdata_journey_pattern_idx_by_line_id(tdata_t *td, char *line_id, uint32_t start_index);

const char *tdata_vehicle_journey_ids_in_journey_pattern(tdata_t *td, uint32_t jp_index);

calendar_t *tdata_vj_masks_for_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_headsign_for_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_line_code_for_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_productcategory_for_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_agency_id_for_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_agency_name_for_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_agency_url_for_journey_pattern(tdata_t *td, uint32_t jp_index);

/* Returns a pointer to the first stoptime for the VehicleJourney. These are generally TimeDemandTypes that must
   be shifted in time to get the true scheduled arrival and departure times. */
stoptime_t *tdata_timedemand_type(tdata_t *td, uint32_t jp_index, uint32_t vj_index);

/* Get a pointer to the array of vehicle_journeys for this journey_pattern. */
vehicle_journey_t *tdata_vehicle_journeys_in_journey_pattern(tdata_t *td, uint32_t jp_index);

const char *tdata_stop_desc_for_index(tdata_t *td, spidx_t stop_index);

rtime_t transfer_duration (tdata_t *tdata, router_request_t *req, spidx_t stop_index_from, spidx_t stop_index_to);

const char *tdata_stop_name_for_index(tdata_t *td, spidx_t stop_index);

#endif /* _TDATA_H */
