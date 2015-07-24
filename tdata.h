/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* tdata.h */

#ifndef _TDATA_H
#define _TDATA_H

#include "config.h"
#include "geometry.h"
#include "rrrr_types.h"

#include "hashgrid.h"

#ifdef RRRR_FEATURE_REALTIME
#include "gtfs-realtime.pb-c.h"
#include "radixtree.h"
#endif

#include <stddef.h>
#include <stdbool.h>

typedef struct tdata tdata_t;
struct tdata {
    void *base;
    size_t size;

    /* Pointer to string pool, with the timezone of the times in the data*/
    uint32_t timezone;

    /* Midnight (UTC) of the first day in the 32-day calendar in seconds since the epoch */
    uint64_t calendar_start_time;

    /* The offset in seconds from UTC for the entire timetable */
    int32_t utc_offset;
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
    uint32_t n_line_colors;
    uint32_t n_line_colors_text;
    uint32_t n_stop_point_ids;
    uint32_t n_stop_area_ids;
    uint32_t n_stop_area_timezones;
    uint32_t n_vj_ids;
    uint32_t n_vj_time_offsets;
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
    spidx_t *transfer_target_stops;
    rtime_t  *transfer_durations;
    rtime_t  *stop_point_waittime;
    rtime_t max_time;
    /* optional data:
     * NULL pointer means it is not available */
    jpidx_t *journey_patterns_at_stop;
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
    lineidx_t *line_for_route;
    opidx_t *operator_for_line;
    vehicle_journey_ref_t *vehicle_journey_transfers_backward;
    vehicle_journey_ref_t *vehicle_journey_transfers_forward;
    char *string_pool;
    uint32_t *line_codes;
    uint32_t *line_names;
    calendar_t *vj_active;
    calendar_t *journey_pattern_active;
    rtime_t *journey_pattern_min;
    rtime_t *journey_pattern_max;
    uint32_t *journey_pattern_point_headsigns;
    uint32_t *line_ids;
    uint32_t *line_colors;
    uint32_t *line_colors_text;
    uint32_t *stop_point_ids;
    uint32_t *stop_area_ids;
    uint32_t *stop_area_timezones;
    int8_t *vj_time_offsets;
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
    /* The latlon lookup for each stop_point */
    hashgrid_t hg;
    coord_t *coords;
};

#ifdef RRRR_DEBUG
void tdata_dump(tdata_t *td);
void tdata_dump_journey_pattern(tdata_t *td, jpidx_t jp_index, jp_vjoffset_t vj_offset);
#endif

/* * * * * * * * * * * * * * * * * * *
 *
 * GENERAL FUNCTIONS
 *
 * * * * * * * * * * * * * * * * * * */

bool tdata_load(tdata_t *td, char *filename);

void tdata_close(tdata_t *td);

/* Return timezone used in general for timetable, eg 'Europe/Amsterdam */
const char *tdata_timezone(tdata_t *td);

bool tdata_hashgrid_setup (tdata_t *tdata);

void tdata_hashgrid_teardown (tdata_t *tdata);

#ifdef RRRR_FEATURE_REALTIME
bool tdata_realtime_setup (tdata_t *tdata);
#endif

/* Return UTC epoch of start and end midnights with the valditiy of the timetable */
void tdata_validity (tdata_t *tdata, uint64_t *min, uint64_t *max);
/* Return the lower-left and upper-right of the extends of all stop_point's in the timetable */
void tdata_extends (tdata_t *tdata, latlon_t *ll, latlon_t *ur);
/* Return the mode enum of all journey_patterns in the timetable */
void tdata_modes (tdata_t *tdata, tmode_t *m);


/* * * * * * * * * * * * * * * * * * *
 *
 *  OPERATOR:
 *  An operator is a company responsible for a line
 *
 * * * * * * * * * * * * * * * * * * */

const char *tdata_operator_id_for_index(tdata_t *td, opidx_t operator_index);

const char *tdata_operator_name_for_index(tdata_t *td, opidx_t operator_index);

const char *tdata_operator_url_for_index(tdata_t *td, opidx_t operator_index);

opidx_t tdata_operator_idx_by_operator_name(tdata_t *td, const char *operator_name, opidx_t operator_index_offset);

opidx_t tdata_operator_idx_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

/* * * * * * * * * * * * * * * * * * *
 *
 *  PHYSICAL_MODE:
 *  Physical_mode is the type of transport used for a line. For example BUS, TRAIN or TRAM, etc.
 *
 * * * * * * * * * * * * * * * * * * */

const char *tdata_name_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index);

const char *tdata_id_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index);

/* * * * * * * * * * * * * * * * * * *
 *
 *  COMMERCIAL_MODE:
 *  Commercial_mode is the type of transport used to market a line. For example R-NET, Intercity, Expressbus, etc.
 *
 * * * * * * * * * * * * * * * * * * */

const char *tdata_name_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index);

const char *tdata_id_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index);


/* * * * * * * * * * * * * * * * * * *
 *
 *  LINE:
 *  An line is a collection of routes, used to communicate a logical path for multiple journeys
 *  by linenumbers, colors and names.
 *
 * * * * * * * * * * * * * * * * * * */

const char *tdata_line_id_for_index(tdata_t *td, lineidx_t line_index);

const char *tdata_line_code_for_index(tdata_t *td, lineidx_t line_index);

const char *tdata_line_color_for_index(tdata_t *td, lineidx_t line_index);

const char *tdata_line_color_text_for_index(tdata_t *td, lineidx_t line_index);

const char *tdata_line_name_for_index(tdata_t *td, lineidx_t line_index);

/* * * * * * * * * * * * * * * * * * *
 *
 *  ROUTE:
 *  An route is a collection of journey_patterns, that all match a common commercially used direction,
 *  This direction is given by the operator and does not necessarily match any physical reality.
 *
 * * * * * * * * * * * * * * * * * * */

/* TODO No function definied for the type, however it would make sense to add id's to data for routes to be able to request them */

/* * * * * * * * * * * * * * * * * * *
 *
 *  JOURNEY_PATTERN:
 *  A journey pattern is an ordered list of stop_points.
 *  Two vehicles that serve exactly the same stop_points in exactly the same order and attributes (forboarding/foralighting/etc.)
 *  belong to to the same journey_pattern.
 *
 * * * * * * * * * * * * * * * * * * */

spidx_t *tdata_points_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

stoptime_t *tdata_stoptimes_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_headsign_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_headsign_for_journey_pattern_point(tdata_t *td, jpidx_t jp_index,jppidx_t jpp_offset);

/* Get a pointer to the array of vehicle_journeys for this journey_pattern. */
vehicle_journey_t *tdata_vehicle_journeys_in_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_line_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

calendar_t *tdata_vj_masks_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_line_code_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_line_color_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_line_color_text_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_line_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_commercial_mode_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_commercial_mode_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_physical_mode_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_physical_mode_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_operator_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_operator_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

const char *tdata_operator_url_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

jpidx_t tdata_journey_pattern_idx_by_line_id(tdata_t *td, char *line_id, jpidx_t start_index);

/* * * * * * * * * * * * * * * * * * *
 *
 *  VEHICLE_JOURNEY:
 * A journey is a single run of a vehicle along a journey_pattern.
 * For example, vehicle bus 23 leaving at 14:20 from stop_area Amsterdam Central Station.
 *
 * * * * * * * * * * * * * * * * * * */

const char *tdata_vehicle_journey_id_for_index(tdata_t *td, vjidx_t vj_index);

const char *tdata_vehicle_journey_id_for_jp_vj_offset(tdata_t *td, jpidx_t jp_index, jp_vjoffset_t vj_offset);

int32_t tdata_utc_offset_for_vj_index(tdata_t *td, vjidx_t vj_index);

int32_t tdata_time_offset_for_vj_index(tdata_t *td, vjidx_t vj_index);

int32_t tdata_utc_offset_for_jp_vj_offset(tdata_t *td, jpidx_t jp_index, jp_vjoffset_t vj_offset);

int32_t tdata_time_offset_for_jp_vj_offset(tdata_t *td, jpidx_t jp_index, jp_vjoffset_t vj_offset);

/* Returns a pointer to the first stoptime for the VehicleJourney. These are generally TimeDemandTypes that must
   be shifted in time to get the true scheduled arrival and departure times. */
stoptime_t *tdata_timedemand_type(tdata_t *td, jpidx_t jp_index, jp_vjoffset_t vj_offset);

/* Parse string with vj offset (within journey_pattern) to jp_vj_offset_t */
bool strtovjoffset (const char *str, tdata_t *td, jpidx_t jp_index, jp_vjoffset_t *vj_offset, char **endptr);

/* * * * * * * * * * * * * * * * * * *
 *
 *  STOP_TIME:
 *  A stop_time represents the time when a vehicle is planned to arrive and to leave a stop_point.
 *
 * * * * * * * * * * * * * * * * * * */

/* Return stop_time in seconds to/from midnight in the time-offset used by tdata */
rtime_t tdata_stoptime_for_index(tdata_t *td, jpidx_t jp_index, jppidx_t jpp_offset, jp_vjoffset_t vj_offset, bool arrival);

/* Return stop_time in seconds to/from midnight local-time */
rtime_t tdata_stoptime_local_for_index(tdata_t *td, jpidx_t jp_index, jppidx_t jpp_offset, jp_vjoffset_t vj_offset, bool arrival);

/* Return stop_time in seconds to/from midnight UTC */
int32_t tdata_stoptime_utc_for_index(tdata_t *td, jpidx_t jp_index, jppidx_t jpp_offset, jp_vjoffset_t vj_offset, bool arrival);


/* * * * * * * * * * * * * * * * * * *
 *
 * STOP_AREA:
 * A stop_area is a collection of stop_points.
 * Generally there are at least two stop_points per stop_area, one per direction of a line.
 * Now think of a hub, you will have more than one line.
 * Therefore your stop_area will contain more than two stop_points.
 * In particular cases your stop_area can contain only one stop_point.
 *
 * * * * * * * * * * * * * * * * * * */

/* Parse string with journey_pattenr index to jp_index*/
bool strtojpidx (const char *str, tdata_t *td, jpidx_t *jp, char **endptr);

latlon_t *tdata_stop_area_coord_for_index(tdata_t *td, spidx_t sa_index);

const char *tdata_stop_area_id_for_index(tdata_t *td, spidx_t sa_index);

const char *tdata_stop_area_name_for_index(tdata_t *td, spidx_t sa_index);

const char *tdata_stop_area_timezone_for_index(tdata_t *td, spidx_t saindex);

spidx_t tdata_stop_areaidx_by_stop_area_name(tdata_t *td, char *stop_area_name, spidx_t sp_index_offset);

spidx_t tdata_stop_areaidx_for_index(tdata_t *td, spidx_t sp_index);

spidx_t tdata_stop_areaidx_by_stop_area_name(tdata_t *td, char *stop_area_name, spidx_t sa_index_offset);

spidx_t tdata_stop_areaidx_by_stop_area_id(tdata_t *td, char *stop_area_name, spidx_t sa_index_offset);

/* * * * * * * * * * * * * * * * * * *
 *
 *  STOP_POINT:
 *  A stop_point is the location where a vehicle can stop.
 *
 * * * * * * * * * * * * * * * * * * */

/* TODO: return number of items and store pointer to beginning, to allow restricted pointers */
uint32_t tdata_journey_patterns_for_stop_point(tdata_t *td, spidx_t sp_index, jpidx_t **jp_ret);

const char *tdata_stop_point_id_for_index(tdata_t *td, spidx_t sp_index);

/* Parse string with stop_point index to sp_index */
bool strtospidx (const char *str, tdata_t *td, spidx_t *sp, char **endptr);

uint8_t *tdata_stop_point_attributes_for_journey_pattern(tdata_t *td, jpidx_t jp_index);

uint8_t *tdata_stop_point_attributes_for_index(tdata_t *td, spidx_t sp_index);

const char *tdata_stop_point_name_for_index(tdata_t *td, spidx_t sp_index);

const char *tdata_stop_point_name_for_index(tdata_t *td, spidx_t sp_index);

latlon_t *tdata_stop_point_coord_for_index(tdata_t *td, spidx_t sp_index);

const char *tdata_platformcode_for_index(tdata_t *td, spidx_t sp_index);

spidx_t tdata_stop_pointidx_by_stop_point_name(tdata_t *td, char *stop_point_name, spidx_t sp_index_offset);

spidx_t tdata_stop_pointidx_by_stop_point_id(tdata_t *td, char *stop_point_id, spidx_t sp_index_offset);

/* Get the minimum waittime a passenger has to wait before transferring to another vehicle */
rtime_t tdata_stop_point_waittime (tdata_t *tdata, spidx_t sp_index);
rtime_t transfer_duration (tdata_t *tdata, router_request_t *req, spidx_t sp_index_from, spidx_t sp_index_to);

#endif /* _TDATA_H */
