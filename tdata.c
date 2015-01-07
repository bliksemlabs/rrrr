/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* tdata.c : handles memory mapped data file containing transit timetable etc. */

/* top, make sure it works alone */
#include "tdata.h"
#include "tdata_io_v3.h"
#include "tdata_validation.h"
#include "rrrr_types.h"
#include "util.h"

#ifdef RRRR_FEATURE_REALTIME_ALERTS
#include "tdata_realtime_alerts.h"
#endif
#ifdef RRRR_FEATURE_REALTIME_EXPANDED
#include "tdata_realtime_expanded.h"
#endif

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

#include "config.h"
#include "bitset.h"

const char *tdata_line_id_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->line_ids + (td->line_ids_width * jp_index);
}

const char *tdata_stop_point_id_for_index(tdata_t *td, spidx_t sp_index) {
    return td->stop_point_ids + (td->stop_point_ids_width * sp_index);
}

uint8_t *tdata_stop_point_attributes_for_index(tdata_t *td, spidx_t sp_index) {
    return td->stop_point_attributes + sp_index;
}

const char *tdata_vehicle_journey_id_for_index(tdata_t *td, uint32_t vj_index) {
    return td->vj_ids + (td->vj_ids_width * vj_index);
}

const char *tdata_vehicle_journey_id_for_jp_vj_index(tdata_t *td, uint32_t jp_index, uint32_t vj_index) {
    return td->vj_ids + (td->vj_ids_width * (td->journey_patterns[jp_index].vj_offset + vj_index));
}

const char *tdata_operator_id_for_index(tdata_t *td, uint32_t operator_index) {
    return td->operator_ids + (td->operator_ids_width * operator_index);
}

const char *tdata_operator_name_for_index(tdata_t *td, uint32_t operator_index) {
    return td->operator_names + (td->operator_names_width * operator_index);
}

const char *tdata_operator_url_for_index(tdata_t *td, uint32_t operator_index) {
    return td->operator_urls + (td->operator_urls_width * operator_index);
}

const char *tdata_line_code_for_index(tdata_t *td, uint32_t line_index) {
    return td->string_pool + td->line_names[line_index];
}

const char *tdata_line_name_for_index(tdata_t *td, uint32_t line_index) {
    return td->string_pool + td->line_names[line_index];
}

const char *tdata_name_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index) {
    return td->commercial_mode_names + (td->commercial_mode_names_width * commercial_mode_index);
}

const char *tdata_id_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index) {
    return td->commercial_mode_ids + (td->commercial_mode_ids_width * commercial_mode_index);
}

const char *tdata_name_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index) {
    return td->physical_mode_names + (td->physical_mode_names_width * physical_mode_index);
}

const char *tdata_id_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index) {
    return td->physical_mode_ids + (td->physical_mode_ids_width * physical_mode_index);
}

const char *tdata_platformcode_for_index(tdata_t *td, spidx_t sp_index) {
    switch (sp_index) {
    case STOP_NONE :
        return NULL;
    case ONBOARD :
        return NULL;
    default :
        return td->platformcodes + (td->platformcodes_width * sp_index);
    }
}

spidx_t tdata_stop_pointidx_by_stop_point_name(tdata_t *td, char *stop_point_name, spidx_t sp_index_offset) {
    spidx_t sp_index;
    for (sp_index = sp_index_offset;
         sp_index < td->n_stop_points;
         ++sp_index) {
        if (strcasestr(td->string_pool + td->stop_point_nameidx[sp_index],
                stop_point_name)) {
            return sp_index;
        }
    }
    return STOP_NONE;
}

spidx_t tdata_stop_areaidx_for_index(tdata_t *td, spidx_t sp_index) {
    return td->stop_area_for_stop_point[sp_index];
}

spidx_t tdata_stop_areaidx_by_stop_area_name(tdata_t *td, char *stop_point_name, spidx_t sa_index_offset) {
    spidx_t sa_index;
    for (sa_index = sa_index_offset;
         sa_index < td->n_stop_areas;
         ++sa_index) {
        if (strcasestr(td->string_pool + td->stop_area_nameidx[sa_index],
                stop_point_name)) {
            return sa_index;
        }
    }
    return STOP_NONE;
}

spidx_t tdata_stop_pointidx_by_stop_point_id(tdata_t *td, char *stop_point_id, spidx_t sp_index_offset) {
    spidx_t sp_index;
    for (sp_index = sp_index_offset;
         sp_index < td->n_stop_points;
         ++sp_index) {
        if (strcasestr(td->stop_point_ids + (td->stop_point_ids_width * sp_index),
                stop_point_id)) {
            return sp_index;
        }
    }
    return STOP_NONE;
}

#define tdata_stop_pointidx_by_stop_point_id(td, stop_id) tdata_stopidx_by_stop_id(td, stop_id, 0)

uint32_t tdata_journey_pattern_idx_by_line_id(tdata_t *td, char *line_id, uint32_t jp_index_offset) {
    uint32_t jp_index;
    for (jp_index = jp_index_offset;
         jp_index < td->n_journey_patterns;
         ++jp_index) {
        if (strcasestr(td->line_ids + (td->line_ids_width * jp_index),
                line_id)) {
            return jp_index;
        }
    }
    return NONE;
}

#define tdata_journey_pattern_idx_by_line_id(td, line_id) tdata_journey_pattern_idx_by_line_id(td, jp_index_offset, 0)

const char *tdata_vehicle_journey_ids_in_journey_pattern(tdata_t *td, uint32_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    uint32_t char_offset = jp->vj_offset * td->vj_ids_width;
    return td->vj_ids + char_offset;
}

calendar_t *tdata_vj_masks_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    return td->vj_active + jp->vj_offset;
}

const char *tdata_headsign_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->string_pool + ((td->journey_pattern_point_headsigns)[(td->journey_patterns)[jp_index].journey_pattern_point_offset]);
}

const char *tdata_headsign_for_journey_pattern_point(tdata_t *td, uint32_t jp_index, uint32_t jpp_index) {
    if (jp_index == NONE) return "NONE";
    return td->string_pool + ((td->journey_pattern_point_headsigns)[(td->journey_patterns)[jp_index].journey_pattern_point_offset + jpp_index]);
}

const char *tdata_line_code_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    uint16_t route_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    return tdata_line_code_for_index(td, td->line_for_route[route_index]);
}

const char *tdata_line_name_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    uint16_t route_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    return tdata_line_name_for_index(td, td->line_for_route[route_index]);
}

const char *tdata_commercial_mode_name_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return tdata_name_for_commercial_mode_index(td,(td->commercial_mode_for_jp)[jp_index]);
}

const char *tdata_commercial_mode_id_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return tdata_id_for_commercial_mode_index(td,(td->commercial_mode_for_jp)[jp_index]);
}

const char *tdata_physical_mode_name_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return tdata_name_for_physical_mode_index(td,(td->physical_mode_for_line)[line_index]);
}

const char *tdata_physical_mode_id_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return tdata_id_for_physical_mode_index(td,(td->physical_mode_for_line)[line_index]);
}

uint32_t tdata_operatoridx_by_operator_name(tdata_t *td, char *operator_name, uint32_t operator_index_offset) {
    uint32_t operator_index;
    for (operator_index = operator_index_offset;
         operator_index < td->n_operator_names;
         ++operator_index) {
        if (strcasestr(td->operator_names + (td->operator_names_width * operator_index),
                operator_name)) {
            return operator_index;
        }
    }
    return NONE;
}

const char *tdata_operator_id_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return td->operator_ids + (td->operator_ids_width * td->operator_for_line[line_index]);
}

const char *tdata_operator_name_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return td->operator_names + (td->operator_names_width * td->operator_for_line[line_index]);
}

const char *tdata_operator_url_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return td->operator_urls + (td->operator_urls_width * td->operator_for_line[line_index]);
}

bool tdata_load(tdata_t *td, char *filename) {
    if ( !tdata_io_v3_load (td, filename)) return false;

    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    if ( !tdata_alloc_expanded (td)) return false;
    #endif

    #ifdef RRRR_FEATURE_REALTIME_ALERTS
    td->alerts = NULL;
    #endif

    /* This is probably a bit slow and is not strictly necessary,
     * but does page in all the timetable entries.
     */
    #ifdef RRRR_STRICT
    return tdata_validation_check_coherent(td);
    #else
    return true;
    #endif
}

void tdata_close(tdata_t *td) {
    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    tdata_free_expanded (td);
    #endif
    #ifdef RRRR_FEATURE_REALTIME_ALERTS
    tdata_clear_gtfsrt_alerts (td);
    #endif

    tdata_io_v3_close (td);
}

spidx_t *tdata_points_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    return td->journey_pattern_points + td->journey_patterns[jp_index].journey_pattern_point_offset;
}

uint8_t *tdata_stop_point_attributes_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    return td->journey_pattern_point_attributes + jp->journey_pattern_point_offset;
}

uint32_t tdata_journey_patterns_for_stop_point(tdata_t *td, spidx_t sp_index, uint32_t **jp_ret) {
    stop_point_t *stop0 = &(td->stop_points[sp_index]);
    stop_point_t *stop1 = &(td->stop_points[sp_index + 1]);
    *jp_ret = td->journey_patterns_at_stop + stop0->journey_patterns_at_stop_point_offset;
    return stop1->journey_patterns_at_stop_point_offset - stop0->journey_patterns_at_stop_point_offset;
}

stoptime_t *tdata_timedemand_type(tdata_t *td, uint32_t jp_index, uint32_t vj_index) {
    return td->stop_times + td->vjs[td->journey_patterns[jp_index].vj_offset + vj_index].stop_times_offset;
}

vehicle_journey_t *tdata_vehicle_journeys_in_journey_pattern(tdata_t *td, uint32_t jp_index) {
    return td->vjs + td->journey_patterns[jp_index].vj_offset;
}

const char *tdata_stop_point_name_for_index(tdata_t *td, spidx_t sp_index) {
    switch (sp_index) {
    case STOP_NONE :
        return "NONE";
    case ONBOARD :
        return "ONBOARD";
    default :
        return td->string_pool + td->stop_point_nameidx[sp_index];
    }
}

const char *tdata_stop_area_name_for_index(tdata_t *td, spidx_t sa_index) {
    return td->string_pool + td->stop_area_nameidx[sa_index];
}

/* Rather than reserving a place to store the transfers used to create the initial state, we look them up as needed. */
rtime_t transfer_duration (tdata_t *tdata, router_request_t *req, spidx_t sp_index_from, spidx_t sp_index_to) {
    UNUSED(req);
    if (sp_index_from != sp_index_to) {
        uint32_t t  = tdata->stop_points[sp_index_from    ].transfers_offset;
        uint32_t tN = tdata->stop_points[sp_index_from + 1].transfers_offset;
        for ( ; t < tN ; ++t) {
            if (tdata->transfer_target_stops[t] == sp_index_to) {
                return (rtime_t) tdata->transfer_durations[t] + req->walk_slack;
            }
        }
    } else {
        return 0;
    }
    return UNREACHED;
}

#ifdef RRRR_DEBUG
void tdata_dump_journey_pattern(tdata_t *td, uint32_t jp_index, uint32_t vj_index) {
    spidx_t *stops = tdata_points_for_journey_pattern(td, jp_index);
    uint32_t ti;
    spidx_t si;
    journey_pattern_t jp = td->journey_patterns[jp_index];
    printf("\njourney_pattern details for %s %s %s '%s %s' [%d] (n_stops %d, n_vjs %d)\n"
           "vjid, stop sequence, stop name (index), departures  \n",
            tdata_operator_name_for_journey_pattern(td, jp_index),
            tdata_operator_id_for_journey_pattern(td, jp_index),
            tdata_operator_url_for_journey_pattern(td, jp_index),
        tdata_line_code_for_journey_pattern(td, jp_index),
        tdata_headsign_for_journey_pattern(td, jp_index),
        jp_index, jp.n_stops, jp.n_vjs);

    for (ti = (vj_index == NONE ? 0 : vj_index);
         ti < (vj_index == NONE ? jp.n_vjs :
                                    vj_index + 1);
         ++ti) {
        stoptime_t *times = tdata_timedemand_type(td, jp_index, ti);
        /* TODO should this really be a 2D array ?
        stoptime_t (*times)[jp.n_stops] = (void*) tdata_timedemand_type(td, jp_index, ti); */

        printf("%s\n", tdata_vehicle_journey_id_for_index(td, jp.vj_offset + ti));
        for (si = 0; si < jp.n_stops; ++si) {
            const char *stop_id = tdata_stop_point_name_for_index (td, stops[si]);
            char arrival[13], departure[13];
            printf("%4d %35s [%06d] : %s %s",
                   si, stop_id, stops[si],
                   btimetext(times[si].arrival + td->vjs[jp.vj_offset + ti].begin_time + RTIME_ONE_DAY, arrival),
                   btimetext(times[si].departure + td->vjs[jp.vj_offset + ti].begin_time + RTIME_ONE_DAY, departure));

            #ifdef RRRR_FEATURE_REALTIME_EXPANDED
            if (td->vj_stoptimes && td->vj_stoptimes[jp.vj_offset + ti]) {
                printf (" %s %s",
                        btimetext(td->vj_stoptimes[jp.vj_offset + ti][si].arrival + RTIME_ONE_DAY, arrival),
                        btimetext(td->vj_stoptimes[jp.vj_offset + ti][si].departure + RTIME_ONE_DAY, departure));
            }
            #endif

            printf("\n");
         }
         printf("\n");
    }
    printf("\n");
}

void tdata_dump(tdata_t *td) {
    uint32_t i;

    printf("\nCONTEXT\n"
           "n_stop_points: %d\n"
           "n_journey_patterns: %d\n", td->n_stop_points, td->n_journey_patterns);
    printf("\nSTOPS\n");
    for (i = 0; i < td->n_stop_points; i++) {
        stop_point_t s0 = td->stop_points[i];
        stop_point_t s1 = td->stop_points[i+1];
        uint32_t j0 = s0.journey_patterns_at_stop_point_offset;
        uint32_t j1 = s1.journey_patterns_at_stop_point_offset;
        uint32_t j;

        printf("stop %d at lat %f lon %f\n",
               i, td->stop_point_coords[i].lat, td->stop_point_coords[i].lon);
        printf("served by journey_patterns ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", td->journey_patterns_at_stop[j]);
        }
        printf("\n");
    }
    printf("\nJOURNEY_PATTERN\n");
    for (i = 0; i < td->n_journey_patterns; i++) {
        journey_pattern_t *r0 = &(td->journey_patterns[i]);
        journey_pattern_t *r1 = &(td->journey_patterns[i+1]);
        uint32_t j0 = r0->journey_pattern_point_offset;
        uint32_t j1 = r1->journey_pattern_point_offset;
        uint32_t j;

        printf("journey_pattern %d\n", i);
        printf("having vjs %d\n", td->journey_patterns[i].n_vjs);
        printf("serves stops ");
        for (j = j0; j < j1; ++j) {
            printf("%d ", td->journey_pattern_points[j]);
        }
        printf("\n");
    }
    printf("\nSTOPIDS\n");
    for (i = 0; i < td->n_stop_points; i++) {
        printf("stop %03d has id %s \n", i, tdata_stop_point_name_for_index(td, i));
    }
    for (i = 0; i < td->n_journey_patterns; i++) {
        /* TODO: Remove?
         * printf("journey_pattern %03d has id %s and first vehicle_journey id %s \n", i,
         *        tdata_route_desc_for_index(td, i),
         *        tdata_vehicle_journey_ids_for_route(td, i));
         */
        tdata_dump_journey_pattern(td, i, NONE);
    }
}
#endif
