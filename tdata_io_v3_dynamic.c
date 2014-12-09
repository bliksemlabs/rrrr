/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"

#ifdef RRRR_TDATA_IO_DYNAMIC

#include "tdata_io_v3.h"
#include "tdata.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


#define load_dynamic(fd, storage, type) \
    td->n_##storage = header->n_##storage; \
    td->storage = (type*) malloc (RRRR_DYNAMIC_SLACK * (td->n_##storage + 1) * sizeof(type)); \
    lseek (fd, header->loc_##storage, SEEK_SET); \
    if (read (fd, td->storage, (td->n_##storage + 1) * sizeof(type)) != (ssize_t) ((td->n_##storage + 1) * sizeof(type))) goto fail_close_fd;

#define load_dynamic_string(fd, storage) \
    td->n_##storage = header->n_##storage; \
    lseek (fd, header->loc_##storage, SEEK_SET); \
    if (read (fd, &td->storage##_width, sizeof(uint32_t)) != sizeof(uint32_t)) goto fail_close_fd; \
    td->storage = (char*) malloc (RRRR_DYNAMIC_SLACK * td->n_##storage * td->storage##_width * sizeof(char)); \
    if (read (fd, td->storage, td->n_##storage * td->storage##_width * sizeof(char)) != (ssize_t) (td->n_##storage * td->storage##_width * sizeof(char))) goto fail_close_fd;

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

    td->calendar_start_time = header->calendar_start_time;
    td->dst_active = header->dst_active;

    load_dynamic (fd, stops, stop_t);
    load_dynamic (fd, stop_attributes, uint8_t);
    load_dynamic (fd, stop_coords, latlon_t);
    load_dynamic (fd, journey_patterns, journey_pattern_t);
    load_dynamic (fd, journey_pattern_points, uint32_t);
    load_dynamic (fd, journey_pattern_point_attributes, uint8_t);
    load_dynamic (fd, stop_times, stoptime_t);
    load_dynamic (fd, trips, trip_t);
    load_dynamic (fd, journey_patterns_at_stop, uint32_t);
    load_dynamic (fd, transfer_target_stops, uint32_t);
    load_dynamic (fd, transfer_dist_meters, uint8_t);
    load_dynamic (fd, trip_active, calendar_t);
    load_dynamic (fd, journey_pattern_active, calendar_t);
    load_dynamic (fd, headsigns, char);
    load_dynamic (fd, stop_names, char);
    load_dynamic (fd, stop_nameidx, uint32_t);

    load_dynamic_string (fd, platformcodes);
    load_dynamic_string (fd, stop_ids);
    load_dynamic_string (fd, trip_ids);
    load_dynamic_string (fd, agency_ids);
    load_dynamic_string (fd, agency_names);
    load_dynamic_string (fd, agency_urls);
    load_dynamic_string (fd, route_shortnames);
    load_dynamic_string (fd, route_ids);
    load_dynamic_string (fd, productcategories);

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
    free (td->trips);
    free (td->journey_patterns_at_stop);
    free (td->transfer_target_stops);
    free (td->transfer_dist_meters);
    free (td->trip_active);
    free (td->journey_pattern_active);
    free (td->headsigns);
    free (td->stop_names);
    free (td->stop_nameidx);

    free (td->platformcodes);
    free (td->stop_ids);
    free (td->trip_ids);
    free (td->agency_ids);
    free (td->agency_names);
    free (td->agency_urls);
    free (td->route_shortnames);
    free (td->route_ids);
    free (td->productcategories);
}

#else
void tdata_io_v3_dynamic_not_available();
#endif /* RRRR_TDATA_IO_DYNAMIC */
