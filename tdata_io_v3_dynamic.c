/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"

#ifdef RRRR_TDATA_IO_DYNAMIC

#include "tdata_io_v3.h"
#include "tdata.h"
#include "rrrr_types.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define load_dynamic(fd, storage, type) \
    td->n_##storage = header->n_##storage; \
    td->storage = (type*) malloc (sizeof(type) * RRRR_DYNAMIC_SLACK * (td->n_##storage + 1)); \
    if (lseek (fd, header->loc_##storage, SEEK_SET) == -1) goto fail_close_fd; \
    if (read (fd, td->storage, sizeof(type) * (td->n_##storage + 1)) != (ssize_t) (sizeof(type) * (td->n_##storage + 1))) goto fail_close_fd;

/* Set the maximum drivetime of any day in tdata */
void set_max_time(tdata_t *td){
    jpidx_t jp_index;
    td->max_time = 0;
    for (jp_index = 0; jp_index < td->n_journey_patterns; jp_index++){
        if (td->journey_patterns[jp_index].max_time > td->max_time) {
            td->max_time = td->journey_patterns[jp_index].max_time;
        }
    }
}

bool tdata_io_v3_load(tdata_t *td, char *filename) {
    tdata_header_t h;
    tdata_header_t *header = &h;

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "The input file %s could not be found.\n", filename);
        return false;
    }

    if (read (fd, header, sizeof(tdata_header_t)) != sizeof(tdata_header_t)) {
        goto fail_close_fd;
    }

    td->base = NULL;
    td->size = 0;

    if( strncmp("TTABLEV4", header->version_string, 8) ) {
        fprintf(stderr, "The input file %s does not appear to be a timetable or is of the wrong version.\n", filename);
        goto fail_close_fd;
    }

    /* More input validation in the dynamic loading case. */
    if ( !( header->n_stop_points < ((spidx_t) -2) &&
            header->n_stop_areas < ((spidx_t) -2) &&
            header->n_stop_point_attributes < ((spidx_t) -2) &&
            header->n_stop_point_coords < ((spidx_t) -2) &&
            header->n_stop_area_coords < ((spidx_t) -2) &&
            header->n_stop_area_coords < ((spidx_t) -2) &&
            header->n_journey_patterns < ((jpidx_t) -1) &&
            header->n_journey_pattern_points < (UINT32_MAX) &&
            header->n_journey_pattern_point_attributes < (UINT32_MAX) &&
            header->n_journey_pattern_point_headsigns < (UINT32_MAX) &&
            header->n_stop_times < (UINT32_MAX) &&
            header->n_vjs < (UINT32_MAX) &&
            header->n_journey_patterns_at_stop < (UINT32_MAX) &&
            header->n_transfer_target_stops < (UINT32_MAX) &&
            header->n_transfer_durations < (UINT32_MAX) &&
            header->n_vj_active < (UINT32_MAX) &&
            header->n_journey_pattern_active < (UINT32_MAX) &&
            header->n_platformcodes < (UINT32_MAX) &&
            header->n_stop_point_nameidx < ((spidx_t) -2) &&
            header->n_operator_ids < (UINT8_MAX) &&
            header->n_operator_names < (UINT8_MAX) &&
            header->n_operator_urls < (UINT8_MAX) &&
            header->n_string_pool < (UINT32_MAX) &&
            header->n_line_codes < (UINT16_MAX) &&
            header->n_commercial_mode_ids < (UINT16_MAX) &&
            header->n_commercial_mode_names < (UINT16_MAX) &&
            header->n_commercial_mode_for_jp < (UINT32_MAX) &&
            header->n_physical_mode_ids < (UINT16_MAX) &&
            header->n_physical_mode_names < (UINT16_MAX) &&
            header->n_physical_mode_for_line < (UINT16_MAX) &&
            header->n_line_ids < (UINT32_MAX) &&
            header->n_line_for_route < (UINT16_MAX) &&
            header->n_stop_point_ids < ((spidx_t) -2) &&
            header->n_stop_area_ids < ((spidx_t) -2) &&
            header->n_vj_ids < (UINT32_MAX) ) ) {

        fprintf(stderr, "The input file %s does not appear to be a valid timetable.\n", filename);
        goto fail_close_fd;
    }

    td->calendar_start_time = header->calendar_start_time;
    td->dst_active = header->dst_active;
    td->n_days = 32;
    td->n_stop_areas = header->n_stop_areas;

    load_dynamic (fd, stop_points, stop_point_t);
    load_dynamic (fd, stop_point_attributes, uint8_t);
    load_dynamic (fd, stop_point_coords, latlon_t);
    load_dynamic (fd, stop_area_coords, latlon_t);
    load_dynamic (fd, journey_patterns, journey_pattern_t);
    load_dynamic (fd, journey_pattern_points, spidx_t);
    load_dynamic (fd, journey_pattern_point_attributes, uint8_t);
    load_dynamic (fd, journey_pattern_point_headsigns, uint32_t);
    load_dynamic (fd, stop_times, stoptime_t);
    load_dynamic (fd, vjs, vehicle_journey_t);
    load_dynamic (fd, journey_patterns_at_stop, jpidx_t);
    load_dynamic (fd, transfer_target_stops, spidx_t);
    load_dynamic (fd, transfer_durations, rtime_t);
    load_dynamic (fd, stop_point_waittime, rtime_t);
    load_dynamic (fd, vehicle_journey_transfers_backward, vehicle_journey_ref_t);
    load_dynamic (fd, vehicle_journey_transfers_forward, vehicle_journey_ref_t);
    load_dynamic (fd, vj_active, calendar_t);
    load_dynamic (fd, journey_pattern_active, calendar_t);
    load_dynamic (fd, string_pool, char);
    load_dynamic (fd, stop_point_nameidx, uint32_t);
    load_dynamic (fd, stop_area_nameidx, uint32_t);
    load_dynamic (fd, line_for_route, uint16_t);
    load_dynamic (fd, operator_for_line, uint8_t);
    load_dynamic (fd, commercial_mode_for_jp, uint16_t);
    load_dynamic (fd, physical_mode_for_line, uint16_t);
    load_dynamic (fd, line_codes, uint32_t);
    load_dynamic (fd, line_names, uint32_t);
    load_dynamic (fd, operator_ids, uint32_t);
    load_dynamic (fd, operator_names, uint32_t);
    load_dynamic (fd, operator_urls, uint32_t);
    load_dynamic (fd, commercial_mode_ids, uint32_t);
    load_dynamic (fd, commercial_mode_names, uint32_t);
    load_dynamic (fd, physical_mode_ids, uint32_t);
    load_dynamic (fd, physical_mode_names, uint32_t);
    load_dynamic (fd, platformcodes, uint32_t);
    load_dynamic (fd, line_ids, uint32_t);
    load_dynamic (fd, stop_point_ids, uint32_t);
    load_dynamic (fd, stop_area_ids, uint32_t);
    load_dynamic (fd, vj_ids, uint32_t);

    set_max_time(td);
    close (fd);

    return true;

fail_close_fd:
    close (fd);

    return false;
}

void tdata_io_v3_close(tdata_t *td) {
    free (td->stop_points);
    free (td->stop_point_attributes);
    free (td->stop_point_coords);
    free (td->stop_area_coords);
    free (td->journey_patterns);
    free (td->journey_pattern_points);
    free (td->journey_pattern_point_attributes);
    free (td->journey_pattern_point_headsigns);
    free (td->stop_times);
    free (td->vjs);
    free (td->journey_patterns_at_stop);
    free (td->transfer_target_stops);
    free (td->transfer_durations);
    free (td->stop_point_waittime);
    free (td->vehicle_journey_transfers_backward);
    free (td->vehicle_journey_transfers_forward);
    free (td->vj_active);
    free (td->journey_pattern_active);
    free (td->string_pool);
    free (td->stop_point_nameidx);
    free (td->stop_area_nameidx);
    free (td->line_for_route);
    free (td->operator_for_line);
    free (td->commercial_mode_for_jp);
    free (td->physical_mode_for_line);

    free (td->platformcodes);
    free (td->stop_point_ids);
    free (td->stop_area_ids);
    free (td->vj_ids);
    free (td->operator_ids);
    free (td->operator_names);
    free (td->operator_urls);
    free (td->line_codes);
    free (td->line_names);
    free (td->line_ids);
    free (td->commercial_mode_ids);
    free (td->commercial_mode_names);
    free (td->physical_mode_ids);
    free (td->physical_mode_names);
}

#else
void tdata_io_v3_dynamic_not_available();
#endif /* RRRR_TDATA_IO_DYNAMIC */
