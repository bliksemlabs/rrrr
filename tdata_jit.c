#include "tdata_jit.h"
#include "index_journey_pattern_points.h"
#include "index_journey_patterns.h"
#include "index_vehicle_journeys.h"

#include <stdlib.h>
#include <string.h>
#include "string_pool.h"

/* this code can't be used because we don't have the memory management in place */

bool tdata_transfers_new (tdata_t *td, uint32_t n) {
    td->n_transfer_durations = n;
    td->n_transfer_target_stops = n;

    td->transfer_target_stops = (spidx_t *) calloc (n, sizeof(spidx_t));
    td->transfer_durations = (rtime_t *) calloc (n, sizeof(rtime_t));

    return (td->transfer_target_stops && td->transfer_durations);
}

bool tdata_transfers_append (tdata_t *td, spidx_t orig, spidx_t *target_stops, rtime_t *durations, uint8_t n_stops) {
    uint32_t n = td->n_transfer_target_stops;

    td->n_transfer_target_stops += n_stops;
    td->n_transfer_durations += n_stops;

    memcpy (&td->transfer_target_stops[n], target_stops, sizeof(spidx_t) * n_stops);
    memcpy (&td->transfer_durations[n], durations, sizeof(spidx_t) * n_stops);

    td->stop_points[orig].transfers_offset = n;
    /* TODO:
    td->stop_transfers_offset[orig] = n;
    td->stop_transfers_len[orig] = n_stop;
    */

    return true;
}

void tdata_transfers_free (tdata_t *td) {
    td->n_transfer_durations = 0;
    td->n_transfer_target_stops = 0;

    free (td->transfer_target_stops);
    free (td->transfer_durations);

    td->transfer_target_stops = NULL;
    td->transfer_durations = NULL;
}


bool tdata_stop_points_new (tdata_t *td, spidx_t n) {
    td->n_stop_points = n;

    td->n_platformcodes            = n;
    td->n_stop_point_ids           = n;
    td->n_stop_point_coords        = n;
    td->n_stop_point_nameidx       = n;
    td->n_stop_point_waittime      = n;
    td->n_stop_point_attributes    = n;
    td->n_stop_area_for_stop_point = n;

    td->stop_points              = (stop_point_t *) calloc (n + 1, sizeof(stop_point_t));
    td->platformcodes            = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->stop_point_ids           = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->stop_point_coords        = (latlon_t *) calloc (n, sizeof(latlon_t));
    td->stop_point_nameidx       = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->stop_point_waittime      = (rtime_t *)  calloc (n, sizeof(rtime_t));
    td->stop_point_attributes    = (uint8_t *)  calloc (n, sizeof(uint8_t));
    td->stop_area_for_stop_point = (spidx_t *)  calloc (n, sizeof(spidx_t));

    return (td->stop_points && td->stop_point_ids && td->stop_point_coords &&
            td->stop_point_nameidx && td->stop_point_waittime &&
            td->stop_point_attributes && td->stop_area_for_stop_point);
}

void tdata_stop_point_append (tdata_t *td,
                              const char *id,
                              const char *name,
                              const char *platformcode,
                              const latlon_t *coord,
                              const rtime_t waittime,
                              const spidx_t stop_area,
                              const uint8_t attributes) {

    spidx_t n = (spidx_t) td->n_stop_points;

    td->n_stop_points++;
    td->n_platformcodes++;
    td->n_stop_point_ids++;
    td->n_stop_point_coords++;
    td->n_stop_point_nameidx++;
    td->n_stop_point_waittime++;
    td->n_stop_point_attributes++;
    td->n_stop_area_for_stop_point++;

    /* Als er geen transfers zijn, waar verwijst de transfer offset dan naar? */

    td->platformcodes[n] = tdata_string_pool_append (td, platformcode);
    td->stop_point_ids[n] = tdata_string_pool_append (td, id);
    memcpy (&td->stop_point_coords[n], coord, sizeof(latlon_t));
    td->stop_point_nameidx[n] = tdata_string_pool_append (td, name);
    td->stop_point_waittime[n] = waittime;
    td->stop_point_attributes[n] = attributes;
    td->stop_area_for_stop_point[n] = stop_area;
}

/* TODO: we could optimise this function, if we implement
 * a custom qsort (or better: swap function) which allows
 * modifications on two columns instead of one.
 * An alternative is creating a list of uint32_t and keep
 * the order in that list, but requires more lookups.
 */
bool tdata_journey_patterns_at_stop (tdata_t *td) {
    uint32_t *journey_patterns_at_stop_point_offset;
    jpidx_t *journey_patterns_at_stop;
    uint32_t n_journey_patterns_at_stop;
    bool result;

    result = index_journey_pattern_points (td, &journey_patterns_at_stop, &n_journey_patterns_at_stop, &journey_patterns_at_stop_point_offset);
    if (result) {
        /* keep legacy compatibility now */
        spidx_t sp_index = (spidx_t) td->n_stop_points;
        while (sp_index > 0) {
            sp_index--;
            td->stop_points[sp_index].journey_patterns_at_stop_point_offset = journey_patterns_at_stop_point_offset[sp_index];
        }
        free (journey_patterns_at_stop_point_offset);

        /* but ideally do the column store dance */
        td->journey_patterns_at_stop = journey_patterns_at_stop;
    }

    return result;
}

void tdata_stop_points_free (tdata_t *td) {
    td->n_stop_points = 0;

    td->n_stop_point_ids           = 0;
    td->n_stop_point_coords        = 0;
    td->n_stop_point_nameidx       = 0;
    td->n_stop_point_waittime      = 0;
    td->n_stop_point_attributes    = 0;
    td->n_stop_area_for_stop_point = 0;

    free (td->stop_points);
    free (td->stop_point_ids);
    free (td->stop_point_coords);
    free (td->stop_point_nameidx);
    free (td->stop_point_waittime);
    free (td->stop_point_attributes);
    free (td->stop_area_for_stop_point);

    td->stop_points              = NULL;
    td->stop_point_ids           = NULL;
    td->stop_point_coords        = NULL;
    td->stop_point_nameidx       = NULL;
    td->stop_point_waittime      = NULL;
    td->stop_point_attributes    = NULL;
    td->stop_area_for_stop_point = NULL;
}

bool tdata_stop_areas_new (tdata_t *td, spidx_t n) {
    td->n_stop_areas = n;

    td->n_stop_area_ids       = n;
    td->n_stop_area_coords    = n;
    td->n_stop_area_nameidx   = n;
    td->n_stop_area_timezones = n;

    td->stop_area_ids       = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->stop_area_coords    = (latlon_t *) calloc (n, sizeof(latlon_t));
    td->stop_area_nameidx   = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->stop_area_timezones = (uint32_t *) calloc (n, sizeof(uint32_t));

    return (td->stop_area_coords && td->stop_area_nameidx &&
            td->stop_area_ids && td->stop_area_timezones);
}

void tdata_stop_area_append (tdata_t *td,
                             const char *id,
                             const char *name,
                             const char *timezone,
                             const latlon_t *coord) {

    /* current highest value */
    spidx_t n = (spidx_t) td->n_stop_areas;

    td->n_stop_areas++;
    td->n_stop_area_ids++;
    td->n_stop_area_coords++;
    td->n_stop_area_nameidx++;
    td->n_stop_area_timezones++;

    td->stop_area_ids[n] = tdata_string_pool_append (td, id);
    memcpy (&td->stop_area_coords[n], coord, sizeof(latlon_t));
    td->stop_area_nameidx[n] = tdata_string_pool_append (td, name);
    td->stop_area_timezones[n] = tdata_string_pool_append (td, timezone);
}

void tdata_stop_areas_free (tdata_t *td) {
    td->n_stop_areas = 0;

    td->n_stop_area_ids       = 0;
    td->n_stop_area_coords    = 0;
    td->n_stop_area_nameidx   = 0;
    td->n_stop_area_timezones = 0;

    free (td->stop_area_ids);
    free (td->stop_area_coords);
    free (td->stop_area_nameidx);
    free (td->stop_area_timezones);

    td->stop_area_ids       = NULL;
    td->stop_area_coords    = NULL;
    td->stop_area_nameidx   = NULL;
    td->stop_area_timezones = NULL;
}

bool tdata_journey_patterns_new (tdata_t *td, jpidx_t n_jp, jppidx_t n_jpp, uint16_t n_r) {
    td->n_journey_patterns = n_jp;

    td->n_line_for_route         = n_r;
    td->n_journey_pattern_active = n_jp;
    td->n_commercial_mode_for_jp = n_jp;

    td->journey_patterns = (journey_pattern_t *) calloc (n_jp, sizeof(journey_pattern_t));
    td->line_for_route   =          (uint16_t *) calloc (n_r,  sizeof(uint16_t));
    td->journey_pattern_active =  (calendar_t *) calloc (n_jp, sizeof(calendar_t));
    td->commercial_mode_for_jp =    (uint16_t *) calloc (n_jp, sizeof(uint16_t));

    td->n_journey_pattern_points = n_jpp;
    td->n_journey_pattern_point_headsigns  = n_jpp;
    td->n_journey_pattern_point_attributes = n_jpp;

    td->journey_pattern_points =            (spidx_t *) calloc (n_jpp, sizeof(spidx_t));
    td->journey_pattern_point_headsigns  = (uint32_t *) calloc (n_jpp, sizeof(uint32_t));
    td->journey_pattern_point_attributes =  (uint8_t *) calloc (n_jpp, sizeof(uint8_t));

    return (td->journey_patterns && td->journey_pattern_active &&
            td->commercial_mode_for_jp &&
            td->journey_pattern_points && td->journey_pattern_point_headsigns &&
            td->journey_pattern_point_attributes);
}

bool tdata_journey_pattern_points_append (tdata_t *td, spidx_t *journey_pattern_points, uint8_t *journey_pattern_point_attributes, uint32_t *journey_pattern_point_headsigns, jppidx_t n_stops) {
    uint32_t n = td->n_journey_pattern_points;

    td->n_journey_pattern_points += n_stops;
    td->n_journey_pattern_point_headsigns += n_stops;
    td->n_journey_pattern_point_attributes += n_stops;

    memcpy (&td->journey_pattern_points[n], journey_pattern_points, sizeof(spidx_t) * n_stops);
    memcpy (&td->journey_pattern_point_headsigns[n], journey_pattern_point_headsigns, sizeof(spidx_t) * n_stops);
    memcpy (&td->journey_pattern_point_attributes[n], journey_pattern_point_attributes, sizeof(spidx_t) * n_stops);

    return true;
}

bool tdata_journey_patterns_append (tdata_t *td, uint32_t jpp_offset, vjidx_t vj_index, jppidx_t n_stops, jp_vjoffset_t n_vjs, uint16_t attributes, routeidx_t route_index, uint16_t commercial_mode, lineidx_t line) {
    jpidx_t n = (jpidx_t) td->n_journey_patterns;

    td->n_journey_patterns++;

    td->journey_patterns[n].journey_pattern_point_offset = jpp_offset;
    td->journey_patterns[n].n_stops = n_stops;
    td->journey_patterns[n].vj_index = vj_index;
    td->journey_patterns[n].n_vjs = n_vjs;
    td->journey_patterns[n].attributes = attributes;
    td->journey_patterns[n].route_index = route_index;

    td->line_for_route[n] = line;
    td->commercial_mode_for_jp[n] = commercial_mode;

    /* if we increase the n_journey_patterns here the index must be updated as well */
    index_vehicle_journeys (td, n, td->journey_pattern_active, td->journey_pattern_min, td->journey_pattern_max);

    return true;
}

bool tdata_journey_patterns_index (tdata_t *td) {
    return index_journey_patterns (td, &td->journey_pattern_active, &td->journey_pattern_min, &td->journey_pattern_max);
}

void tdata_journey_patterns_free (tdata_t *td) {
    td->n_line_for_route                   = 0;
    td->n_journey_patterns                 = 0;
    td->n_journey_pattern_active           = 0;
    td->n_commercial_mode_for_jp           = 0;
    td->n_journey_pattern_points           = 0;
    td->n_journey_pattern_point_attributes = 0;
    td->n_journey_pattern_point_headsigns  = 0;

    free (td->journey_patterns);
    free (td->journey_patterns);
    free (td->journey_pattern_active);
    free (td->commercial_mode_for_jp);
    free (td->journey_pattern_points);
    free (td->journey_pattern_point_headsigns);
    free (td->journey_pattern_point_attributes);

    td->journey_patterns                 = NULL;
    td->journey_pattern_active           = NULL;
    td->commercial_mode_for_jp           = NULL;
    td->journey_pattern_points           = NULL;
    td->journey_pattern_point_headsigns  = NULL;
    td->journey_pattern_point_attributes = NULL;
}

bool tdata_vehicle_journeys_new (tdata_t *td, vjidx_t n) {
    td->n_vjs             = n;
    td->n_vj_ids          = n;
    td->n_vj_active       = n;
    td->n_vj_time_offsets = n;
    td->n_vehicle_journey_transfers_forward  = n;
    td->n_vehicle_journey_transfers_backward = n;

    td->vjs =      (vehicle_journey_t *) calloc (n, sizeof(vehicle_journey_t));
    td->vj_ids          =   (uint32_t *) calloc (n, sizeof(uint32_t));
    td->vj_active       = (calendar_t *) calloc (n, sizeof(uint32_t));
    td->vj_time_offsets =     (int8_t *) calloc (n, sizeof(int8_t));
    td->vehicle_journey_transfers_forward  = (vehicle_journey_ref_t *) calloc (n, sizeof(vehicle_journey_ref_t));
    td->vehicle_journey_transfers_backward = (vehicle_journey_ref_t *) calloc (n, sizeof(vehicle_journey_ref_t));

    return (td->vjs && td->vj_ids && td->vj_active &&
            td->vj_time_offsets &&
            td->vehicle_journey_transfers_forward &&
            td->vehicle_journey_transfers_backward);
}

bool tdata_vehicle_journey_append (tdata_t *td,
                                   const char *id,
                                   const uint32_t stop_times_offset,
                                   const rtime_t begin_time,
                                   const vj_attribute_mask_t attributes,
                                   const calendar_t active,
                                   const int8_t time_offset,
                                   const vehicle_journey_ref_t transfer_forward,
                                   const vehicle_journey_ref_t transfer_backward) {
    vjidx_t n = (vjidx_t) td->n_vjs;

    td->n_vjs++;
    td->n_vj_ids++;
    td->n_vj_active++;
    td->n_vj_time_offsets++;
    td->n_vehicle_journey_transfers_forward++;
    td->n_vehicle_journey_transfers_backward++;

    td->vjs[n].stop_times_offset = stop_times_offset;
    td->vjs[n].begin_time = begin_time;
    td->vjs[n].vj_attributes = attributes;

    td->vj_ids[n]          = tdata_string_pool_append (td, id);
    td->vj_active[n]       = active;
    td->vj_time_offsets[n] = time_offset;
    td->vehicle_journey_transfers_forward[n]  = transfer_forward;
    td->vehicle_journey_transfers_backward[n] = transfer_backward;

    return true;
}

void tdata_vehicle_journeys_free (tdata_t *td) {
    td->n_vjs             = 0;
    td->n_vj_ids          = 0;
    td->n_vj_active       = 0;
    td->n_vj_time_offsets = 0;
    td->n_vehicle_journey_transfers_forward  = 0;
    td->n_vehicle_journey_transfers_backward = 0;

    free (td->vjs);
    free (td->vj_ids);
    free (td->vj_active);
    free (td->vj_time_offsets);
    free (td->vehicle_journey_transfers_forward);
    free (td->vehicle_journey_transfers_backward);

    td->vjs             = NULL;
    td->vj_ids          = NULL;
    td->vj_active       = NULL;
    td->vj_time_offsets = NULL;
    td->vehicle_journey_transfers_forward  = NULL;
    td->vehicle_journey_transfers_backward = NULL;
}

bool tdata_operators_new (tdata_t *td, uint8_t n) {
    td->n_operator_ids   = n;
    td->n_operator_urls  = n;
    td->n_operator_names = n;

    td->operator_ids   = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->operator_urls  = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->operator_names = (uint32_t *) calloc (n, sizeof(uint32_t));

    return (td->operator_ids && td->operator_urls && td->operator_names);
}

bool tdata_operators_append (tdata_t *td,
                             const char *id,
                             const char *url,
                             const char *name) {
    opidx_t n = (opidx_t) td->n_operator_ids;
    td->n_operator_ids++;
    td->n_operator_urls++;
    td->n_operator_names++;

    td->operator_ids[n]   = tdata_string_pool_append (td, id);
    td->operator_urls[n]  = tdata_string_pool_append (td, url);
    td->operator_names[n] = tdata_string_pool_append (td, name);

    return true;
}

void tdata_operators_free (tdata_t *td) {
    td->n_operator_ids   = 0;
    td->n_operator_urls  = 0;
    td->n_operator_names = 0;

    free (td->operator_ids);
    free (td->operator_urls);
    free (td->operator_names);

    td->operator_ids   = NULL;
    td->operator_urls  = NULL;
    td->operator_names = NULL;
}

bool tdata_commercial_modes_new (tdata_t *td, uint16_t n) {
    td->n_commercial_mode_ids   = n;
    td->n_commercial_mode_names = n;

    td->commercial_mode_ids   = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->commercial_mode_names = (uint32_t *) calloc (n, sizeof(uint32_t));

    return (td->commercial_mode_ids && td->commercial_mode_names);
}

bool tdata_commercial_modes_append (tdata_t *td,
                                    const char *id,
                                    const char *name) {
    uint16_t n = (uint16_t) td->n_commercial_mode_ids;
    td->n_commercial_mode_ids++;
    td->n_commercial_mode_names++;

    td->commercial_mode_ids[n]   = tdata_string_pool_append (td, id);
    td->commercial_mode_names[n] = tdata_string_pool_append (td, name);

    return true;
}

void tdata_commercial_modes_free (tdata_t *td) {
    td->n_commercial_mode_ids   = 0;
    td->n_commercial_mode_names = 0;

    free (td->commercial_mode_ids);
    free (td->commercial_mode_names);

    td->commercial_mode_ids   = NULL;
    td->commercial_mode_names = NULL;
}

bool tdata_physical_modes_new (tdata_t *td, uint16_t n) {
    td->n_physical_mode_ids   = n;
    td->n_physical_mode_names = n;

    td->physical_mode_ids   = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->physical_mode_names = (uint32_t *) calloc (n, sizeof(uint32_t));

    return (td->physical_mode_ids && td->physical_mode_names);
}

bool tdata_physical_modes_append (tdata_t *td,
                                  const char *id,
                                  const char *name) {
    uint16_t n = (uint16_t) td->n_physical_mode_ids;

    td->n_physical_mode_ids++;
    td->n_physical_mode_names++;

    td->physical_mode_ids[n]   = tdata_string_pool_append (td, id);
    td->physical_mode_names[n] = tdata_string_pool_append (td, name);

    return true;
}

void tdata_physical_modes_free (tdata_t *td) {
    td->n_physical_mode_ids   = 0;
    td->n_physical_mode_names = 0;

    free (td->physical_mode_ids);
    free (td->physical_mode_names);

    td->physical_mode_ids   = NULL;
    td->physical_mode_names = NULL;
}

bool tdata_lines_new (tdata_t *td, uint32_t n) {
    td->n_line_ids               = n;
    td->n_line_codes             = n;
    td->n_line_names             = n;
    td->n_line_colors            = n;
    td->n_line_colors_text       = n;
    td->n_operator_for_line      = n;
    td->n_physical_mode_for_line = n;

    td->line_ids               = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->line_codes             = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->line_names             = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->line_colors            = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->line_colors_text       = (uint32_t *) calloc (n, sizeof(uint32_t));
    td->operator_for_line      =  (uint8_t *) calloc (n, sizeof(uint8_t));
    td->physical_mode_for_line = (uint16_t *) calloc (n, sizeof(uint16_t));

    return (td->line_ids && td->line_codes && td->line_names &&
            td->line_colors && td->line_colors_text &&
            td->physical_mode_for_line);
}

bool tdata_line_append (tdata_t *td,
                        const char *id,
                        const char *code,
                        const char *name,
                        const char *color,
                        const char *color_text,
                        const uint8_t operator,
                        const uint16_t physical_mode) {

    uint16_t n = (uint16_t) td->n_line_ids;

    td->n_physical_mode_ids++;
    td->n_line_codes++;
    td->n_line_names++;
    td->n_line_colors++;
    td->n_line_colors_text++;
    td->n_operator_for_line++;
    td->n_physical_mode_for_line++;

    td->line_ids[n] = tdata_string_pool_append (td, id);
    td->line_codes[n] = tdata_string_pool_append (td, code);
    td->line_names[n] = tdata_string_pool_append (td, name);
    td->line_colors[n] = tdata_string_pool_append (td, color);
    td->line_colors_text[n] = tdata_string_pool_append (td, color_text);

    td->operator_for_line[n] = operator;
    td->physical_mode_for_line[n] = physical_mode;

    return true;
}

void tdata_lines_free (tdata_t *td) {
    td->n_line_ids               = 0;
    td->n_line_codes             = 0;
    td->n_line_names             = 0;
    td->n_line_colors            = 0;
    td->n_line_colors_text       = 0;
    td->n_operator_for_line      = 0;
    td->n_physical_mode_for_line = 0;

    free (td->line_ids);
    free (td->line_codes);
    free (td->line_names);
    free (td->line_colors);
    free (td->line_colors_text);
    free (td->operator_for_line);
    free (td->physical_mode_for_line);

    td->line_ids               = NULL;
    td->line_codes             = NULL;
    td->line_names             = NULL;
    td->line_colors            = NULL;
    td->line_colors_text       = NULL;
    td->operator_for_line      = NULL;
    td->physical_mode_for_line = NULL;
}

bool tdata_stop_times_new (tdata_t *td, uint32_t n) {
    td->n_stop_times = n;

    td->stop_times = (stoptime_t *) calloc (n, sizeof(stoptime_t));

    return (td->stop_times);
}

bool tdata_stop_times_append (tdata_t *td, stoptime_t *stop_times, uint32_t n_stop_times) {
    uint32_t n = td->n_stop_times;

    td->n_stop_times += n_stop_times;

    memcpy (&td->stop_times[n], stop_times, sizeof(stoptime_t) * n_stop_times);

    return true;
}

void tdata_stop_times_free (tdata_t *td) {
    td->n_stop_times = 0;

    free (td->stop_times);

    td->stop_times = NULL;
}

bool tdata_string_pool_new (tdata_t *td, uint32_t n) {
    td->n_string_pool = n;

    td->string_pool = (char *) calloc (n, sizeof(char));

    return (td->string_pool);
}

uint32_t tdata_string_pool_append (tdata_t *td, const char *str) {
    return string_pool_append(td->string_pool, &td->n_string_pool, td->stringpool_index, str);
}

void tdata_string_pool_free (tdata_t *td) {
    td->n_string_pool = 0;

    free (td->string_pool);

    td->string_pool = NULL;
}

bool tdata_jit_new (tdata_t *td,
                    const char *timezone,
                    uint64_t calendar_start_time,
                    int32_t utc_offset,
                    rtime_t max_time,
                    uint32_t n_days
                    ) {
    tdata_string_pool_new (td, 65535);

    td->timezone = tdata_string_pool_append (td, timezone);
    td->calendar_start_time = calendar_start_time;
    td->utc_offset = utc_offset;
    td->max_time = max_time;
    td->n_days = n_days;

    return true;
}

void tdata_jit_free (tdata_t *td) {
    /* tdata_route_free (td); */

    tdata_lines_free (td);
    tdata_physical_modes_free (td);
    tdata_journey_patterns_free (td);
    tdata_commercial_modes_free (td);
    tdata_vehicle_journeys_free (td);
    tdata_stop_times_free (td);

    tdata_stop_points_free (td);
    tdata_stop_areas_free (td);
    tdata_journey_patterns_free (td);

    free (td);
}
