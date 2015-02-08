/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* tdata.h */

#ifndef _TDATA_H
#define _TDATA_H

#include "config.h"
#include "geometry.h"
#include "rrrr_types.h"

#ifdef RRRR_FEATURE_LATLON
#include "hashgrid.h"
#endif

#ifdef RRRR_FEATURE_REALTIME
#include "gtfs-realtime.pb-c.h"
#include "radixtree.h"
#endif

#include <stddef.h>
#include <stdbool.h>

typedef struct stop_point stop_point_t;
struct stop_point {
    uint32_t journey_patterns_at_stop_point_offset;
    uint32_t transfers_offset;
};

/* An individual JourneyPattern in the RAPTOR sense:
 * A group of VehicleJourneys all share the same JourneyPattern.
 */
typedef struct journey_pattern journey_pattern_t;
struct journey_pattern {
    uint32_t journey_pattern_point_offset;
    uint32_t vj_offset;
    jppidx_t n_stops;
    jp_vjoffset_t n_vjs;
    uint16_t attributes;
    uint16_t route_index;
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

typedef struct vehicle_journey_ref vehicle_journey_ref_t;
struct vehicle_journey_ref {
    jppidx_t journey_pattern_index;
    jp_vjoffset_t vehicle_journey_index;
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
    uint32_t n_days;
    uint32_t n_stop_points;
    uint32_t n_stop_areas;
    uint32_t n_stop_point_attributes;
    uint32_t n_stop_point_coords;
    uint32_t n_stop_area_coords;
    uint32_t n_stop_area_for_stop_point;
    uint32_t n_journey_patterns;
    uint32_t n_journey_pattern_points;
    uint32_t n_journey_pattern_point_attributes;
    uint32_t n_journey_pattern_point_headsigns;
    uint32_t n_stop_times;
    uint32_t n_vjs;
    uint32_t n_journey_patterns_at_stop;
    uint32_t n_transfer_target_stops;
    uint32_t n_transfer_durations;
    uint32_t n_vj_active;
    uint32_t n_journey_pattern_active;
    uint32_t n_platformcodes;
    uint32_t n_stop_point_names;
    uint32_t n_stop_point_nameidx;
    uint32_t n_stop_area_nameidx;
    uint32_t n_operator_ids;
    uint32_t n_operator_names;
    uint32_t n_operator_urls;
    uint32_t n_commercial_mode_ids;
    uint32_t n_commercial_mode_names;
    uint32_t n_physical_mode_ids;
    uint32_t n_physical_mode_names;
    uint32_t n_string_pool;
    uint32_t n_line_codes;
    uint32_t n_line_names;
    uint32_t n_line_ids;
    uint32_t n_stop_point_ids;
    uint32_t n_stop_area_ids;
    uint32_t n_vj_ids;
    uint32_t n_line_for_route;
    uint32_t n_operator_for_line;
    uint32_t n_commercial_mode_for_jp;
    uint32_t n_physical_mode_for_line;
    uint32_t n_vehicle_journey_transfers_backward;
    uint32_t n_vehicle_journey_transfers_forward;
    uint32_t n_stop_point_waittime;
    stop_point_t *stop_points;
    uint8_t *stop_point_attributes;
    journey_pattern_t *journey_patterns;
    spidx_t *journey_pattern_points;
    uint8_t  *journey_pattern_point_attributes;
    stoptime_t *stop_times;
    vehicle_journey_t *vjs;
    jpidx_t *journey_patterns_at_stop;
    spidx_t *transfer_target_stops;
    rtime_t  *transfer_durations;
    rtime_t  *stop_point_waittime;
    rtime_t max_time;
    /* optional data:
     * NULL pointer means it is not available */
    latlon_t *stop_point_coords;
    latlon_t *stop_area_coords;
    spidx_t *stop_area_for_stop_point;
    uint32_t *platformcodes;
    uint32_t *stop_point_nameidx;
    uint32_t *stop_area_nameidx;
    uint32_t *operator_ids;
    uint32_t *operator_names;
    uint32_t *operator_urls;
    uint32_t *commercial_mode_ids;
    uint32_t *commercial_mode_names;
    uint32_t *physical_mode_ids;
    uint32_t *physical_mode_names;
    uint16_t *commercial_mode_for_jp;
    uint16_t *physical_mode_for_line;
    uint16_t *line_for_route;
    uint8_t *operator_for_line;
    vehicle_journey_ref_t *vehicle_journey_transfers_backward;
    vehicle_journey_ref_t *vehicle_journey_transfers_forward;
    char *string_pool;
    uint32_t *line_codes;
    uint32_t *line_names;
    calendar_t *vj_active;
    calendar_t *journey_pattern_active;
    uint32_t *journey_pattern_point_headsigns;
    uint32_t *line_ids;
    uint32_t *stop_point_ids;
    uint32_t *stop_area_ids;
    uint32_t *vj_ids;
    #ifdef RRRR_FEATURE_REALTIME
    radixtree_t *lineid_index;
    radixtree_t *stop_point_id_index;
    radixtree_t *vjid_index;
    radixtree_t *stringpool_index;
    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    stoptime_t **vj_stoptimes;
    uint32_t *vjs_in_journey_pattern;
    list_t **rt_journey_patterns_at_stop_point;
    calendar_t *vj_active_orig;
    calendar_t *journey_pattern_active_orig;
    #endif
    #ifdef RRRR_FEATURE_REALTIME_ALERTS
    TransitRealtime__FeedMessage *alerts;
    #endif
    #endif
    #ifdef RRRR_FEATURE_LATLON
    /* The latlon lookup for each stop_point */
    hashgrid_t hg;
    #endif
};

bool tdata_load(tdata_t *td, char *filename);

void tdata_close(tdata_t *td);

#ifdef RRRR_DEBUG
void tdata_dump(tdata_t *td);
#endif

spidx_t *tdata_points_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

uint8_t *tdata_stop_point_attributes_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

spidx_t tdata_stop_areaidx_for_index(tdata_t *td, spidx_t sp_index);

spidx_t tdata_stop_areaidx_by_stop_area_name(tdata_t *td, char *stop_point_name, spidx_t sa_index_offset);

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
uint32_t tdata_journey_patterns_for_stop_point(tdata_t *td, spidx_t sp_index, jpidx_t **jp_ret);

stoptime_t *tdata_stoptimes_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

#ifdef RRRR_DEBUG
void tdata_dump_journey_pattern(tdata_t *td, jpidx_t jp_index, uint32_t vj_index);
#endif

const char *tdata_line_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_stop_point_id_for_index(tdata_t *td, spidx_t sp_index);

uint8_t *tdata_stop_point_attributes_for_index(tdata_t *td, spidx_t sp_index);

const char *tdata_vehicle_journey_id_for_index(tdata_t *td, uint32_t vj_index);

const char *tdata_vehicle_journey_id_for_jp_vj_index(tdata_t *td, jpidx_t jp_index, uint32_t vj_index);

uint32_t tdata_operatoridx_by_operator_name(tdata_t *td, char *operator_name, uint32_t start_index);

const char *tdata_operator_id_for_index(tdata_t *td, uint32_t operator_index);

const char *tdata_operator_name_for_index(tdata_t *td, uint32_t operator_index);

const char *tdata_operator_url_for_index(tdata_t *td, uint32_t operator_index);

const char *tdata_line_code_for_index(tdata_t *td, uint32_t line_code_index);

const char *tdata_line_name_for_index(tdata_t *td, uint32_t line_name_index);

const char *tdata_line_id_for_index(tdata_t *td, uint32_t line_name_index);

const char *tdata_name_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index);

const char *tdata_id_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index);

const char *tdata_name_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index);

const char *tdata_id_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index);

const char *tdata_stop_point_name_for_index(tdata_t *td, spidx_t sp_index);

const char *tdata_stop_point_name_for_index(tdata_t *td, spidx_t sp_index);

const char *tdata_stop_area_name_for_index(tdata_t *td, spidx_t sp_index);

const char *tdata_platformcode_for_index(tdata_t *td, spidx_t sp_index);

spidx_t tdata_stop_pointidx_by_stop_point_name(tdata_t *td, char *stop_point_name, spidx_t sp_index_offset);

spidx_t tdata_stop_pointidx_by_stop_area_name(tdata_t *td, char *stop_point_name, spidx_t sp_index_offset);

spidx_t tdata_stop_pointidx_by_stop_point_id(tdata_t *td, char *stop_point_id, spidx_t sp_index_offset);

jpidx_t tdata_journey_pattern_idx_by_line_id(tdata_t *td, char *line_id, jpidx_t start_index);

calendar_t *tdata_vj_masks_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_headsign_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_headsign_for_journey_pattern_point(tdata_t *td, jpidx_t jp_index,jppidx_t jpp_offset);

const char *tdata_line_code_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_line_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_commercial_mode_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_commercial_mode_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_physical_mode_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_physical_mode_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_operator_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_operator_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_operator_url_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

/* Returns a pointer to the first stoptime for the VehicleJourney. These are generally TimeDemandTypes that must
   be shifted in time to get the true scheduled arrival and departure times. */
stoptime_t *tdata_timedemand_type(tdata_t *td, jpidx_t jp_index, jp_vjoffset_t vj_index);

/* Get a pointer to the array of vehicle_journeys for this journey_pattern. */
vehicle_journey_t *tdata_vehicle_journeys_in_journey_pattern(tdata_t *td, jpidx_t jp_index);

/* Get the minimum waittime a passenger has to wait before transferring to another vehicle */
rtime_t tdata_stop_point_waittime (tdata_t *tdata, spidx_t sp_index);
rtime_t transfer_duration (tdata_t *tdata, router_request_t *req, spidx_t sp_index_from, spidx_t sp_index_to);

const char *tdata_stop_point_name_for_index(tdata_t *td, spidx_t sp_index);

#ifdef RRRR_FEATURE_LATLON
bool tdata_hashgrid_setup (tdata_t *tdata);
#endif

bool strtospidx (const char *str, tdata_t *td, spidx_t *sp, char **endptr);
bool strtojpidx (const char *str, tdata_t *td, jpidx_t *jp, char **endptr);
bool strtovjoffset (const char *str, tdata_t *td, jpidx_t jp_index, jp_vjoffset_t *vj_o, char **endptr);

#ifdef RRRR_FEATURE_REALTIME
bool tdata_realtime_setup (tdata_t *tdata);
#endif

void tdata_validity (tdata_t *tdata, uint64_t *min, uint64_t *max);
void tdata_extends (tdata_t *tdata, latlon_t *ll, latlon_t *ur);
void tdata_modes (tdata_t *tdata, tmode_t *m);

#endif /* _TDATA_H */
