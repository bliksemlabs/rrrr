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

#define load_dynamic_string(fd, storage) \
    td->n_##storage = header->n_##storage; \
    if (lseek (fd, header->loc_##storage, SEEK_SET) == -1) goto fail_close_fd; \
    if (read (fd, &td->storage##_width, sizeof(uint32_t)) != sizeof(uint32_t)) goto fail_close_fd; \
    if (!(td->storage##_width > 0 && td->storage##_width < UINT16_MAX)) goto fail_close_fd; \
    td->storage = (char*) malloc (((uint64_t) sizeof(char)) * RRRR_DYNAMIC_SLACK * td->n_##storage * td->storage##_width); \
    if (!td->storage) goto fail_close_fd; \
    if (read (fd, td->storage, ((uint64_t) sizeof(char)) * td->n_##storage * td->storage##_width) != (ssize_t) (((uint64_t) sizeof(char)) * td->n_##storage * td->storage##_width)) goto fail_close_fd;

/* Set the maximum drivetime of any day in tdata */
void set_max_time(tdata_t *td){
    uint32_t jp_index;
    td->max_time = UNREACHED;
    for (jp_index = 0; jp_index < td->n_journey_patterns; jp_index++){
        if (td->journey_patterns[jp_index].max_time < td->max_time) {
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

    if( strncmp("TTABLEV3", header->version_string, 8) ) {
        fprintf(stderr, "The input file %s does not appear to be a timetable or is of the wrong version.\n", filename);
        goto fail_close_fd;
    }

    /* More input validation in the dynamic loading case. */
    if ( !( header->n_stops < ((spidx_t) -2) &&
            header->n_stop_attributes < ((spidx_t) -2) &&
            header->n_stop_coords < ((spidx_t) -2) &&
            header->n_journey_patterns < (UINT32_MAX - 1) &&
            header->n_journey_pattern_points < (UINT32_MAX) &&
            header->n_journey_pattern_point_attributes < (UINT32_MAX) &&
            header->n_stop_times < (UINT32_MAX) &&
            header->n_vjs < (UINT32_MAX) &&
            header->n_journey_patterns_at_stop < (UINT32_MAX) &&
            header->n_transfer_target_stops < (UINT32_MAX) &&
            header->n_transfer_dist_meters < (UINT32_MAX) &&
            header->n_vj_active < (UINT32_MAX) &&
            header->n_journey_pattern_active < (UINT32_MAX) &&
            header->n_platformcodes < (UINT32_MAX) &&
            header->n_stop_names < (UINT32_MAX) &&
            header->n_stop_nameidx < ((spidx_t) -2) &&
            header->n_agency_ids < (UINT16_MAX) &&
            header->n_agency_names < (UINT16_MAX) &&
            header->n_agency_urls < (UINT16_MAX) &&
            header->n_headsigns < (UINT32_MAX) &&
            header->n_line_codes < (UINT16_MAX) &&
            header->n_productcategories < (UINT16_MAX) &&
            header->n_line_ids < (UINT32_MAX) &&
            header->n_stop_ids < ((spidx_t) -2) &&
            header->n_vj_ids < (UINT32_MAX) ) ) {

        fprintf(stderr, "The input file %s does not appear to be a valid timetable.\n", filename);
        goto fail_close_fd;
    }

    td->calendar_start_time = header->calendar_start_time;
    td->dst_active = header->dst_active;

    load_dynamic (fd, stops, stop_t);
    load_dynamic (fd, stop_attributes, uint8_t);
    load_dynamic (fd, stop_coords, latlon_t);
    load_dynamic (fd, journey_patterns, journey_pattern_t);
    load_dynamic (fd, journey_pattern_points, spidx_t);
    load_dynamic (fd, journey_pattern_point_attributes, uint8_t);
    load_dynamic (fd, stop_times, stoptime_t);
    load_dynamic (fd, vjs, vehicle_journey_t);
    load_dynamic (fd, journey_patterns_at_stop, uint32_t);
    load_dynamic (fd, transfer_target_stops, spidx_t);
    load_dynamic (fd, transfer_dist_meters, uint8_t);
    load_dynamic (fd, vj_active, calendar_t);
    load_dynamic (fd, journey_pattern_active, calendar_t);
    load_dynamic (fd, headsigns, char);
    load_dynamic (fd, stop_names, char);
    load_dynamic (fd, stop_nameidx, uint32_t);

    load_dynamic_string (fd, platformcodes);
    load_dynamic_string (fd, stop_ids);
    load_dynamic_string (fd, vj_ids);
    load_dynamic_string (fd, agency_ids);
    load_dynamic_string (fd, agency_names);
    load_dynamic_string (fd, agency_urls);
    load_dynamic_string (fd, line_codes);
    load_dynamic_string (fd, line_ids);
    load_dynamic_string (fd, productcategories);
    
    set_max_time(td);
    close (fd);

    return true;

fail_close_fd:
    close (fd);

    return false;
}

void tdata_io_v3_close(tdata_t *td) {
    free (td->stops);
    free (td->stop_attributes);
    free (td->stop_coords);
    free (td->journey_patterns);
    free (td->journey_pattern_points);
    free (td->journey_pattern_point_attributes);
    free (td->stop_times);
    free (td->vjs);
    free (td->journey_patterns_at_stop);
    free (td->transfer_target_stops);
    free (td->transfer_dist_meters);
    free (td->vj_active);
    free (td->journey_pattern_active);
    free (td->headsigns);
    free (td->stop_names);
    free (td->stop_nameidx);

    free (td->platformcodes);
    free (td->stop_ids);
    free (td->vj_ids);
    free (td->agency_ids);
    free (td->agency_names);
    free (td->agency_urls);
    free (td->line_codes);
    free (td->line_ids);
    free (td->productcategories);
}

#else
void tdata_io_v3_dynamic_not_available();
#endif /* RRRR_TDATA_IO_DYNAMIC */
