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

const char *tdata_line_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    uint16_t route_index;
    if (jp_index == NONE) return "NONE";
    route_index = td->journey_patterns[jp_index].route_index;
    return tdata_line_id_for_index(td,td->line_for_route[route_index]);
}

const char *tdata_stop_point_id_for_index(tdata_t *td, spidx_t sp_index) {
    return td->string_pool + td->stop_point_ids[sp_index];
}

uint8_t *tdata_stop_point_attributes_for_index(tdata_t *td, spidx_t sp_index) {
    return td->stop_point_attributes + sp_index;
}

const char *tdata_vehicle_journey_id_for_index(tdata_t *td, uint32_t vj_index) {
    return td->string_pool + td->vj_ids[vj_index];
}

const char *tdata_vehicle_journey_id_for_jp_vj_index(tdata_t *td, jpidx_t jp_index, uint32_t vj_index) {
    return td->string_pool +  td->vj_ids[td->journey_patterns[jp_index].vj_offset + vj_index];
}

const char *tdata_operator_id_for_index(tdata_t *td, uint32_t operator_index) {
    return td->string_pool + (td->operator_ids[operator_index]);
}

const char *tdata_operator_name_for_index(tdata_t *td, uint32_t operator_index) {
    return td->string_pool + (td->operator_names[operator_index]);
}

const char *tdata_operator_url_for_index(tdata_t *td, uint32_t operator_index) {
    return td->string_pool + (td->operator_urls[operator_index]);
}

const char *tdata_line_code_for_index(tdata_t *td, uint32_t line_index) {
    return td->string_pool + td->line_codes[line_index];
}

const char *tdata_line_name_for_index(tdata_t *td, uint32_t line_index) {
    if (td->line_names == NULL) return NULL;
    return td->string_pool + td->line_names[line_index];
}

const char *tdata_line_id_for_index(tdata_t *td, uint32_t line_index) {
    if (td->line_names == NULL) return NULL;
    return td->string_pool + td->line_ids[line_index];
}

const char *tdata_name_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index) {
    return td->string_pool + (td->commercial_mode_names[commercial_mode_index]);
}

const char *tdata_id_for_commercial_mode_index(tdata_t *td, uint32_t commercial_mode_index) {
    return td->string_pool + (td->commercial_mode_ids[commercial_mode_index]);
}

const char *tdata_name_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index) {
    return td->string_pool + (td->physical_mode_names[physical_mode_index]);
}

const char *tdata_id_for_physical_mode_index(tdata_t *td, uint32_t physical_mode_index) {
    return td->string_pool + (td->physical_mode_ids[physical_mode_index]);
}

const char *tdata_platformcode_for_index(tdata_t *td, spidx_t sp_index) {
    switch (sp_index) {
    case STOP_NONE :
        return NULL;
    case ONBOARD :
        return NULL;
    default :
        return td->string_pool + td->platformcodes[sp_index];
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
        if (strcasestr(tdata_stop_point_id_for_index(td, sp_index),
                stop_point_id)) {
            return sp_index;
        }
    }
    return STOP_NONE;
}

#define tdata_stop_pointidx_by_stop_point_id(td, stop_id) tdata_stopidx_by_stop_id(td, stop_id, 0)

jpidx_t tdata_journey_pattern_idx_by_line_id(tdata_t *td, char *line_id, jpidx_t jp_index_offset) {
    jpidx_t jp_index;
    for (jp_index = jp_index_offset;
         jp_index < td->n_journey_patterns;
         ++jp_index) {
        if (strcasestr(tdata_line_id_for_journey_pattern(td, jp_index),
                line_id)) {
            return jp_index;
        }
    }
    return NONE;
}

#define tdata_journey_pattern_idx_by_line_id(td, line_id) tdata_journey_pattern_idx_by_line_id(td, jp_index_offset, 0)

calendar_t *tdata_vj_masks_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    return td->vj_active + jp->vj_offset;
}

const char *tdata_headsign_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return td->string_pool + ((td->journey_pattern_point_headsigns)[(td->journey_patterns)[jp_index].journey_pattern_point_offset]);
}

const char *tdata_headsign_for_journey_pattern_point(tdata_t *td, jpidx_t jp_index, jppidx_t jpp_offset) {
    if (jp_index == NONE) return "NONE";
    return td->string_pool + ((td->journey_pattern_point_headsigns)[(td->journey_patterns)[jp_index].journey_pattern_point_offset + jpp_offset]);
}

const char *tdata_line_code_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    uint16_t route_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    return tdata_line_code_for_index(td, td->line_for_route[route_index]);
}

const char *tdata_line_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    uint16_t route_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    return tdata_line_name_for_index(td, td->line_for_route[route_index]);
}

const char *tdata_commercial_mode_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return tdata_name_for_commercial_mode_index(td,(td->commercial_mode_for_jp)[jp_index]);
}

const char *tdata_commercial_mode_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    if (jp_index == NONE) return "NONE";
    return tdata_id_for_commercial_mode_index(td,(td->commercial_mode_for_jp)[jp_index]);
}

const char *tdata_physical_mode_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return tdata_name_for_physical_mode_index(td,(td->physical_mode_for_line)[line_index]);
}

const char *tdata_physical_mode_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
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
        if (strcasestr(tdata_operator_name_for_index(td, operator_index),
                operator_name)) {
            return operator_index;
        }
    }
    return NONE;
}

const char *tdata_operator_id_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return tdata_operator_id_for_index(td, td->operator_for_line[line_index]);
}

const char *tdata_operator_name_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return tdata_operator_name_for_index(td, td->operator_for_line[line_index]);
}

const char *tdata_operator_url_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    uint16_t route_index,line_index;
    if (jp_index == NONE) return "NONE";
    route_index = (td->journey_patterns)[jp_index].route_index;
    line_index = (td->line_for_route)[route_index];
    return tdata_operator_url_for_index(td, td->operator_for_line[line_index]);
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
    #ifdef RRRR_FEATURE_LATLON
    hashgrid_teardown (&td->hg);
    #endif

    #ifdef RRRR_FEATURE_REALTIME
    if (td->stop_point_id_index) radixtree_destroy (td->stop_point_id_index);
    if (td->vjid_index) radixtree_destroy (td->vjid_index);
    if (td->lineid_index) radixtree_destroy (td->lineid_index);

    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    tdata_free_expanded (td);
    #endif
    #ifdef RRRR_FEATURE_REALTIME_ALERTS
    tdata_clear_gtfsrt_alerts (td);
    #endif
    #endif
    tdata_io_v3_close (td);
}

spidx_t *tdata_points_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    return td->journey_pattern_points + td->journey_patterns[jp_index].journey_pattern_point_offset;
}

uint8_t *tdata_stop_point_attributes_for_journey_pattern(tdata_t *td, jpidx_t jp_index) {
    journey_pattern_t *jp = &(td->journey_patterns[jp_index]);
    return td->journey_pattern_point_attributes + jp->journey_pattern_point_offset;
}

uint32_t tdata_journey_patterns_for_stop_point(tdata_t *td, spidx_t sp_index, jpidx_t **jp_ret) {
    stop_point_t *stop0 = &(td->stop_points[sp_index]);
    stop_point_t *stop1 = &(td->stop_points[sp_index + 1]);
    *jp_ret = td->journey_patterns_at_stop + stop0->journey_patterns_at_stop_point_offset;
    return stop1->journey_patterns_at_stop_point_offset - stop0->journey_patterns_at_stop_point_offset;
}

stoptime_t *tdata_timedemand_type(tdata_t *td, jpidx_t jp_index, jp_vjoffset_t vj_index) {
    return td->stop_times + td->vjs[td->journey_patterns[jp_index].vj_offset + vj_index].stop_times_offset;
}

vehicle_journey_t *tdata_vehicle_journeys_in_journey_pattern(tdata_t *td, jpidx_t jp_index) {
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

rtime_t tdata_stop_point_waittime (tdata_t *tdata, spidx_t sp_index) {
    return tdata->stop_point_waittime[sp_index];
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
void tdata_dump_journey_pattern(tdata_t *td, jpidx_t jp_index, uint32_t vj_index) {
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

#ifdef RRRR_FEATURE_LATLON
bool tdata_hashgrid_setup (tdata_t *tdata) {
    coord_t *coords;
    uint32_t i_sp;

    coords = (coord_t *) malloc(sizeof(coord_t) * tdata->n_stop_points);
    if (!coords) return false;

    i_sp = tdata->n_stop_points;
    do {
        i_sp--;
        coord_from_latlon(coords + i_sp,
                          tdata->stop_point_coords + i_sp);
    } while(i_sp);

    hashgrid_init (&tdata->hg, 100, 500.0, coords, tdata->n_stop_points);

    return true;
}
#endif

bool strtospidx (const char *str, tdata_t *td, spidx_t *sp, char **endptr) {
    long stop_idx = strtol(str, endptr, 10);
    if (stop_idx >= 0 && stop_idx < td->n_stop_points) {
        *sp = (spidx_t) stop_idx;
        return true;
    }
    return false;
}

bool strtojpidx (const char *str, tdata_t *td, jpidx_t *jp, char **endptr) {
    long jp_idx = strtol(str, endptr, 10);
    if (jp_idx >= 0 && jp_idx < td->n_journey_patterns) {
        *jp = (jpidx_t) jp_idx;
        return true;
    }
    return false;
}

bool strtovjoffset (const char *str, tdata_t *td, jpidx_t jp_index, jp_vjoffset_t *vj_o, char **endptr) {
    long vj_offset = strtol(str, endptr, 10);
    if (vj_offset >= 0 && vj_offset < td->journey_patterns[jp_index].n_vjs) {
        *vj_o = (jp_vjoffset_t) vj_offset;
        return true;
    }
    return false;
}

#ifdef RRRR_FEATURE_REALTIME
static
radixtree_t *tdata_radixtree_string_pool_setup (tdata_t *td, uint32_t *s, uint32_t n) {
    uint32_t idx;
    radixtree_t *r = radixtree_new();
    for (idx = 0; idx < n; idx++) {
        radixtree_insert (r, td->string_pool + s[idx], n);
    }
    return r;
}

static
radixtree_t *tdata_radixtree_full_string_pool_setup (char *strings, uint32_t n) {
    radixtree_t *r = radixtree_new();
    char *strings_end = strings + n;
    char *s = strings;
    uint32_t idx = 0;
    while (s < strings_end) {
        size_t width = strlen(s) + 1;
        radixtree_insert (r, s, idx);
        idx += width;
        s += width;
    }

    return r;
}

bool tdata_realtime_setup (tdata_t *tdata) {
    tdata->stop_point_id_index = tdata_radixtree_string_pool_setup (tdata, tdata->stop_point_ids, tdata->n_stop_points);
    tdata->vjid_index = tdata_radixtree_string_pool_setup (tdata, tdata->vj_ids, tdata->n_vjs);
    tdata->lineid_index = tdata_radixtree_string_pool_setup (tdata, tdata->line_ids, tdata->n_line_ids);
    tdata->stringpool_index = tdata_radixtree_full_string_pool_setup (tdata->string_pool, tdata->n_string_pool);

    /* Validate the radixtrees are actually created. */
    return (tdata->stop_point_id_index &&
            tdata->vjid_index &&
            tdata->lineid_index);
}
#endif

void tdata_validity (tdata_t *tdata, uint64_t *min, uint64_t *max) {
    *min = tdata->calendar_start_time;
    *max = tdata->calendar_start_time + (tdata->n_days - 1) * 86400;
}

void tdata_extends (tdata_t *tdata, latlon_t *ll, latlon_t *ur) {
    spidx_t i_stop = (spidx_t) tdata->n_stop_points;

    float min_lon = 180.0f, min_lat = 90.0f, max_lon = -180.0f, max_lat = -90.f;

    do {
        i_stop--;
        if (tdata->stop_point_coords[i_stop].lat == 0.0f ||
            tdata->stop_point_coords[i_stop].lon == 0.0f) continue;

        max_lat = MAX(tdata->stop_point_coords[i_stop].lat, max_lat);
        max_lon = MAX(tdata->stop_point_coords[i_stop].lon, max_lon);
        min_lat = MIN(tdata->stop_point_coords[i_stop].lat, min_lat);
        min_lon = MIN(tdata->stop_point_coords[i_stop].lon, min_lon);
    } while (i_stop > 0);

    ll->lat = min_lat;
    ll->lon = min_lon;
    ur->lat = max_lat;
    ur->lon = max_lon;
}

void tdata_modes (tdata_t *tdata, tmode_t *m) {
    jpidx_t i_jp = (jpidx_t) tdata->n_journey_patterns;
    uint16_t attributes = 0;

    do {
        i_jp--;
        attributes |= tdata->journey_patterns[i_jp].attributes;
    } while (i_jp > 0);

    *m = (tmode_t) attributes;
}
