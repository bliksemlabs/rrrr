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

const char *tdata_stop_id_for_index(tdata_t *td, spidx_t stop_index) {
    return td->stop_ids + (td->stop_ids_width * stop_index);
}

uint8_t *tdata_stop_attributes_for_index(tdata_t *td, spidx_t stop_index) {
    return td->stop_attributes + stop_index;
}

const char *tdata_vehicle_journey_id_for_index(tdata_t *td, uint32_t vj_index) {
    return td->vj_ids + (td->vj_ids_width * vj_index);
}

const char *tdata_vehicle_journey_id_for_jp_vj_index(tdata_t *td, uint32_t jp_index, uint32_t vj_index) {
    return td->vj_ids + (td->vj_ids_width * (td->journey_patterns[jp_index].vj_ids_offset + vj_index));
}

const char *tdata_agency_id_for_index(tdata_t *td, uint32_t agency_index) {
    return td->agency_ids + (td->agency_ids_width * agency_index);
}

const char *tdata_agency_name_for_index(tdata_t *td, uint32_t agency_index) {
    return td->agency_names + (td->agency_names_width * agency_index);
}

const char *tdata_agency_url_for_index(tdata_t *td, uint32_t agency_index) {
    return td->agency_urls + (td->agency_urls_width * agency_index);
}

const char *tdata_headsign_for_offset(tdata_t *td, uint32_t headsign_offset) {
    return td->headsigns + headsign_offset;
}

const char *tdata_line_code_for_index(tdata_t *td, uint32_t line_code_index) {
    return td->line_codes + (td->line_codes_width * line_code_index);
}

const char *tdata_productcategory_for_index(tdata_t *td, uint32_t productcategory_index) {
    return td->productcategories + (td->productcategories_width * productcategory_index);
}

const char *tdata_platformcode_for_index(tdata_t *td, spidx_t stop_index) {
    switch (stop_index) {
    case STOP_NONE :
        return NULL;
    case ONBOARD :
        return NULL;
    default :
        return td->platformcodes + (td->platformcodes_width * stop_index);
    }
}

spidx_t tdata_stopidx_by_stop_name(tdata_t *td, char* stop_name, spidx_t stop_index_offset) {
    spidx_t stop_index;
    for (stop_index = stop_index_offset;
         stop_index < td->n_stops;
         ++stop_index) {
        if (strcasestr(td->stop_names + td->stop_nameidx[stop_index],
                       stop_name)) {
            return stop_index;
        }
    }
    return STOP_NONE;
}

spidx_t tdata_stopidx_by_stop_id(tdata_t *td, char* stop_id, spidx_t stop_index_offset) {
    spidx_t stop_index;
    for (stop_index = stop_index_offset;
         stop_index < td->n_stops;
         ++stop_index) {
        if (strcasestr(td->stop_ids + (td->stop_ids_width * stop_index),
                       stop_id)) {
            return stop_index;
        }
    }
    return STOP_NONE;
}

#define tdata_stopidx_by_stop_id(td, stop_id) tdata_stopidx_by_stop_id(td, stop_id, 0)

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

#define tdata_journey_pattern_idx_by_line_id(td, line_id) tdata_journey_pattern_idx_by_line_id(td, jp_index, 0)

const char *tdata_vehicle_journey_ids_in_journey_pattern(tdata_t *td, uint32_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    uint32_t char_offset = jp->vj_ids_offset * td->vj_ids_width;
    return td->vj_ids + char_offset;
}

calendar_t *tdata_vj_masks_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    return td->vj_active + jp->vj_ids_offset;
}

const char *tdata_headsign_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->headsigns + (td->journey_patterns)[jp_index].headsign_offset;
}

const char *tdata_line_code_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->line_codes + (td->line_codes_width * (td->journey_patterns)[jp_index].line_code_index);
}

const char *tdata_productcategory_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->productcategories + (td->productcategories_width * (td->journey_patterns)[jp_index].productcategory_index);
}

uint32_t tdata_agencyidx_by_agency_name(tdata_t *td, char* agency_name, uint32_t agency_index_offset) {
    uint32_t agency_index;
    for (agency_index = agency_index_offset;
         agency_index < td->n_agency_names;
         ++agency_index) {
        if (strcasestr(td->agency_names + (td->agency_names_width * agency_index),
                       agency_name)) {
            return agency_index;
        }
    }
    return NONE;
}

const char *tdata_agency_id_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->agency_ids + (td->agency_ids_width * (td->journey_patterns)[jp_index].agency_index);
}

const char *tdata_agency_name_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->agency_names + (td->agency_names_width * (td->journey_patterns)[jp_index].agency_index);
}

const char *tdata_agency_url_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->agency_urls + (td->agency_urls_width * (td->journey_patterns)[jp_index].agency_index);
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

uint8_t *tdata_stop_attributes_for_journey_pattern(tdata_t *td, uint32_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    return td->journey_pattern_point_attributes + jp->journey_pattern_point_offset;
}

uint32_t tdata_journey_patterns_for_stop(tdata_t *td, spidx_t stop_index, uint32_t **jp_ret) {
    stop_t *stop0 = &(td->stops[stop_index]);
    stop_t *stop1 = &(td->stops[stop_index + 1]);
    *jp_ret = td->journey_patterns_at_stop + stop0->journey_patterns_at_stop_offset;
    return stop1->journey_patterns_at_stop_offset - stop0->journey_patterns_at_stop_offset;
}

stoptime_t *tdata_timedemand_type(tdata_t *td, uint32_t jp_index, uint32_t vj_index) {
    return td->stop_times + td->vjs[td->journey_patterns[jp_index].vj_ids_offset + vj_index].stop_times_offset;
}

vehicle_journey_t *tdata_vehicle_journeys_in_journey_pattern(tdata_t *td, uint32_t jp_index) {
    return td->vjs + td->journey_patterns[jp_index].vj_ids_offset;
}

const char *tdata_stop_name_for_index(tdata_t *td, spidx_t stop_index) {
    switch (stop_index) {
    case STOP_NONE :
        return "NONE";
    case ONBOARD :
        return "ONBOARD";
    default :
        return td->stop_names + td->stop_nameidx[stop_index];
    }
}

/* Rather than reserving a place to store the transfers used to create the initial state, we look them up as needed. */
rtime_t transfer_duration (tdata_t *tdata, router_request_t *req, spidx_t stop_index_from, spidx_t stop_index_to) {
    UNUSED(req);
    if (stop_index_from != stop_index_to) {
        uint32_t t  = tdata->stops[stop_index_from    ].transfers_offset;
        uint32_t tN = tdata->stops[stop_index_from + 1].transfers_offset;
        for ( ; t < tN ; ++t) {
            if (tdata->transfer_target_stops[t] == stop_index_to) {
                return (rtime_t) tdata->transfer_dist_meters[t] + req->walk_slack;
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
        tdata_agency_name_for_journey_pattern(td, jp_index),
        tdata_agency_id_for_journey_pattern(td, jp_index),
        tdata_agency_url_for_journey_pattern(td, jp_index),
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

        printf("%s\n", tdata_vehicle_journey_id_for_index(td, jp.vj_ids_offset + ti));
        for (si = 0; si < jp.n_stops; ++si) {
            const char *stop_id = tdata_stop_name_for_index (td, stops[si]);
            char arrival[13], departure[13];
            printf("%4d %35s [%06d] : %s %s",
                   si, stop_id, stops[si],
                   btimetext(times[si].arrival + td->vjs[jp.vj_ids_offset + ti].begin_time + RTIME_ONE_DAY, arrival),
                   btimetext(times[si].departure + td->vjs[jp.vj_ids_offset + ti].begin_time + RTIME_ONE_DAY, departure));

            #ifdef RRRR_FEATURE_REALTIME_EXPANDED
            if (td->vj_stoptimes && td->vj_stoptimes[jp.vj_ids_offset + ti]) {
                printf (" %s %s",
                        btimetext(td->vj_stoptimes[jp.vj_ids_offset + ti][si].arrival + RTIME_ONE_DAY, arrival),
                        btimetext(td->vj_stoptimes[jp.vj_ids_offset + ti][si].departure + RTIME_ONE_DAY, departure));
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
           "n_stops: %d\n"
           "n_journey_patterns: %d\n", td->n_stops, td->n_journey_patterns);
    printf("\nSTOPS\n");
    for (i = 0; i < td->n_stops; i++) {
        stop_t s0 = td->stops[i];
        stop_t s1 = td->stops[i+1];
        uint32_t j0 = s0.journey_patterns_at_stop_offset;
        uint32_t j1 = s1.journey_patterns_at_stop_offset;
        uint32_t j;

        printf("stop %d at lat %f lon %f\n",
               i, td->stop_coords[i].lat, td->stop_coords[i].lon);
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
    for (i = 0; i < td->n_stops; i++) {
        printf("stop %03d has id %s \n", i, tdata_stop_name_for_index(td, i));
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
