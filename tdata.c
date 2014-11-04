/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* tdata.c : handles memory mapped data file containing transit timetable etc. */

/* top, make sure it works alone */
#include "tdata.h"
#include "tdata_io_v3.h"
#include "tdata_validation.h"
#include "tdata_realtime.h"

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
#include "stubs.h"

char *tdata_route_id_for_index(tdata_t *td, uint32_t route_index) {
    if (route_index == NONE) return "NONE";
    return td->route_ids + (td->route_ids_width * route_index);
}

char *tdata_stop_id_for_index(tdata_t *td, uint32_t stop_index) {
    return td->stop_ids + (td->stop_ids_width * stop_index);
}

uint8_t *tdata_stop_attributes_for_index(tdata_t *td, uint32_t stop_index) {
    return td->stop_attributes + stop_index;
}

char *tdata_trip_id_for_index(tdata_t *td, uint32_t trip_index) {
    return td->trip_ids + (td->trip_ids_width * trip_index);
}

char *tdata_trip_id_for_route_trip_index(tdata_t *td, uint32_t route_index, uint32_t trip_index) {
    return td->trip_ids + (td->trip_ids_width * (td->routes[route_index].trip_ids_offset + trip_index));
}

char *tdata_agency_id_for_index(tdata_t *td, uint32_t agency_index) {
    return td->agency_ids + (td->agency_ids_width * agency_index);
}

char *tdata_agency_name_for_index(tdata_t *td, uint32_t agency_index) {
    return td->agency_names + (td->agency_names_width * agency_index);
}

char *tdata_agency_url_for_index(tdata_t *td, uint32_t agency_index) {
    return td->agency_urls + (td->agency_urls_width * agency_index);
}

char *tdata_headsign_for_offset(tdata_t *td, uint32_t headsign_offset) {
    return td->headsigns + headsign_offset;
}

char *tdata_route_shortname_for_index(tdata_t *td, uint32_t route_shortname_index) {
    return td->route_shortnames + (td->route_shortnames_width * route_shortname_index);
}

char *tdata_productcategory_for_index(tdata_t *td, uint32_t productcategory_index) {
    return td->productcategories + (td->productcategories_width * productcategory_index);
}

char *tdata_platformcode_for_index(tdata_t *td, uint32_t stop_index) {
    switch (stop_index) {
    case NONE :
        return NULL;
    case ONBOARD :
        return NULL;
    default :
        return td->platformcodes + (td->platformcodes_width * stop_index);
    }
}

uint32_t tdata_stopidx_by_stop_name(tdata_t *td, char* stop_name, uint32_t stop_index_offset) {
    uint32_t stop_index;
    for (stop_index = stop_index_offset;
         stop_index < td->n_stops;
         ++stop_index) {
        if (strcasestr(td->stop_names + td->stop_nameidx[stop_index],
                       stop_name)) {
            return stop_index;
        }
    }
    return NONE;
}

uint32_t tdata_stopidx_by_stop_id(tdata_t *td, char* stop_id, uint32_t stop_index_offset) {
    uint32_t stop_index;
    for (stop_index = stop_index_offset;
         stop_index < td->n_stops;
         ++stop_index) {
        if (strcasestr(td->stop_ids + (td->stop_ids_width * stop_index),
                       stop_id)) {
            return stop_index;
        }
    }
    return NONE;
}

#define tdata_stopidx_by_stop_id(td, stop_id) tdata_stopidx_by_stop_id(td, stop_id, 0)

uint32_t tdata_routeidx_by_route_id(tdata_t *td, char* route_id, uint32_t route_index_offset) {
    uint32_t route_index;
    for (route_index = route_index_offset;
         route_index < td->n_routes;
         ++route_index) {
        if (strcasestr(td->route_ids + (td->route_ids_width * route_index),
                       route_id)) {
            return route_index;
        }
    }
    return NONE;
}

#define tdata_routeidx_by_route_id(td, route_id) tdata_routeidx_by_route_id(td, route_id, 0)

char *tdata_trip_ids_for_route(tdata_t *td, uint32_t route_index) {
    route_t route = (td->routes)[route_index];
    uint32_t char_offset = route.trip_ids_offset * td->trip_ids_width;
    return td->trip_ids + char_offset;
}

calendar_t *tdata_trip_masks_for_route(tdata_t *td, uint32_t route_index) {
    route_t route = (td->routes)[route_index];
    return td->trip_active + route.trip_ids_offset;
}

char *tdata_headsign_for_route(tdata_t *td, uint32_t route_index) {
    if (route_index == NONE) return "NONE";
    return td->headsigns + (td->routes)[route_index].headsign_offset;
}

char *tdata_shortname_for_route(tdata_t *td, uint32_t route_index) {
    if (route_index == NONE) return "NONE";
    return td->route_shortnames + (td->route_shortnames_width * (td->routes)[route_index].shortname_index);
}

char *tdata_productcategory_for_route(tdata_t *td, uint32_t route_index) {
    if (route_index == NONE) return "NONE";
    return td->productcategories + (td->productcategories_width * (td->routes)[route_index].productcategory_index);
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

#define tdata_agencyidx_by_route_name(td, agency_name) tdata_routeidx_by_route_id(td, agency_name, 0)

char *tdata_agency_id_for_route(tdata_t *td, uint32_t route_index) {
    if (route_index == NONE) return "NONE";
    return td->agency_ids + (td->agency_ids_width * (td->routes)[route_index].agency_index);
}

char *tdata_agency_name_for_route(tdata_t *td, uint32_t route_index) {
    if (route_index == NONE) return "NONE";
    return td->agency_names + (td->agency_names_width * (td->routes)[route_index].agency_index);
}

char *tdata_agency_url_for_route(tdata_t *td, uint32_t route_index) {
    if (route_index == NONE) return "NONE";
    return td->agency_urls + (td->agency_urls_width * (td->routes)[route_index].agency_index);
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
    return tdata_validation_check_coherent(td);
}

void tdata_close(tdata_t *td) {
    tdata_io_v3_close (td);
    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    tdata_free_expanded (td);
    #endif
}

uint32_t *tdata_stops_for_route(tdata_t *td, uint32_t route) {
    return td->route_stops + td->routes[route].route_stops_offset;
}

uint8_t *tdata_stop_attributes_for_route(tdata_t *td, uint32_t route) {
    route_t route0 = td->routes[route];
    return td->route_stop_attributes + route0.route_stops_offset;
}

uint32_t tdata_routes_for_stop(tdata_t *td, uint32_t stop, uint32_t **routes_ret) {
    stop_t stop0 = td->stops[stop];
    stop_t stop1 = td->stops[stop + 1];
    *routes_ret = td->stop_routes + stop0.stop_routes_offset;
    return stop1.stop_routes_offset - stop0.stop_routes_offset;
}

stoptime_t *tdata_timedemand_type(tdata_t *td, uint32_t route_index, uint32_t trip_index) {
    return td->stop_times + td->trips[td->routes[route_index].trip_ids_offset + trip_index].stop_times_offset;
}

trip_t *tdata_trips_for_route (tdata_t *td, uint32_t route_index) {
    return td->trips + td->routes[route_index].trip_ids_offset;
}

char *tdata_stop_name_for_index(tdata_t *td, uint32_t stop_index) {
    switch (stop_index) {
    case NONE :
        return "NONE";
    case ONBOARD :
        return "ONBOARD";
    default :
        return td->stop_names + td->stop_nameidx[stop_index];
    }
}

/* Rather than reserving a place to store the transfers used to create the initial state, we look them up as needed. */
rtime_t transfer_duration (tdata_t *tdata, router_request_t *req, uint32_t stop_index_from, uint32_t stop_index_to) {
    if (stop_index_from != stop_index_to) {
        uint32_t t  = tdata->stops[stop_index_from    ].transfers_offset;
        uint32_t tN = tdata->stops[stop_index_from + 1].transfers_offset;
        for ( ; t < tN ; ++t) {
            if (tdata->transfer_target_stops[t] == stop_index_to) {
                uint32_t distance_meters = tdata->transfer_dist_meters[t] << 4; /* actually in units of 16 meters */
                return SEC_TO_RTIME((uint32_t)(distance_meters / req->walk_speed + req->walk_slack));
            }
        }
    } else {
        return 0;
    }
    return UNREACHED;
}

uint32_t transfer_distance (tdata_t *tdata, uint32_t stop_index_from, uint32_t stop_index_to) {
    if (stop_index_from != stop_index_to) {
        uint32_t t  = tdata->stops[stop_index_from    ].transfers_offset;
        uint32_t tN = tdata->stops[stop_index_from + 1].transfers_offset;
        for ( ; t < tN ; ++t) {
            if (tdata->transfer_target_stops[t] == stop_index_to) {
                return tdata->transfer_dist_meters[t] << 4; /* actually in units of 16 meters */
            }
        }
    } else {
        return 0;
    }
    return UNREACHED;
}

#ifdef RRRR_DEBUG
void tdata_dump_route(tdata_t *td, uint32_t route_index, uint32_t trip_index) {
    uint32_t ti, si;
    uint32_t *stops = tdata_stops_for_route(td, route_index);
    route_t route = td->routes[route_index];
    printf("\nRoute details for %s %s %s '%s %s' [%d] (n_stops %d, n_trips %d)\n"
           "tripid, stop sequence, stop name (index), departures  \n",
        tdata_agency_name_for_route(td, route_index),
        tdata_agency_id_for_route(td, route_index),
        tdata_agency_url_for_route(td, route_index),
        tdata_shortname_for_route(td, route_index),
        tdata_headsign_for_route(td, route_index),
        route_index, route.n_stops, route.n_trips);

    for (ti = (trip_index == NONE ? 0 : trip_index);
         ti < (trip_index == NONE ? route.n_trips :
                                    trip_index + 1);
         ++ti) {
        stoptime_t *times = tdata_timedemand_type(td, route_index, ti);
        /* TODO should this really be a 2D array ?
        stoptime_t (*times)[route.n_stops] = (void*) tdata_timedemand_type(td, route_index, ti); */

        printf("%s\n", tdata_trip_id_for_index(td, route.trip_ids_offset + ti));
        for (si = 0; si < route.n_stops; ++si) {
            char *stop_id = tdata_stop_name_for_index (td, stops[si]);
            char arrival[13], departure[13];
            printf("%4d %35s [%06d] : %s %s",
                   si, stop_id, stops[si],
                   btimetext(times[si].arrival + td->trips[route.trip_ids_offset + ti].begin_time + RTIME_ONE_DAY, arrival),
                   btimetext(times[si].departure + td->trips[route.trip_ids_offset + ti].begin_time + RTIME_ONE_DAY, departure));

            #ifdef RRRR_FEATURE_REALTIME_EXPANDED
            if (td->trip_stoptimes && td->trip_stoptimes[route.trip_ids_offset + ti]) {
                printf (" %s %s",
                        btimetext(td->trip_stoptimes[route.trip_ids_offset + ti][si].arrival + RTIME_ONE_DAY, arrival),
                        btimetext(td->trip_stoptimes[route.trip_ids_offset + ti][si].departure + RTIME_ONE_DAY, departure));
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
           "n_routes: %d\n", td->n_stops, td->n_routes);
    printf("\nSTOPS\n");
    for (i = 0; i < td->n_stops; i++) {
        stop_t s0 = td->stops[i];
        stop_t s1 = td->stops[i+1];
        uint32_t j0 = s0.stop_routes_offset;
        uint32_t j1 = s1.stop_routes_offset;
        uint32_t j;

        printf("stop %d at lat %f lon %f\n",
               i, td->stop_coords[i].lat, td->stop_coords[i].lon);
        printf("served by routes ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", td->stop_routes[j]);
        }
        printf("\n");
    }
    printf("\nROUTES\n");
    for (i = 0; i < td->n_routes; i++) {
        route_t r0 = td->routes[i];
        route_t r1 = td->routes[i+1];
        uint32_t j0 = r0.route_stops_offset;
        uint32_t j1 = r1.route_stops_offset;
        uint32_t j;

        printf("route %d\n", i);
        printf("having trips %d\n", td->routes[i].n_trips);
        printf("serves stops ");
        for (j = j0; j < j1; ++j) {
            printf("%d ", td->route_stops[j]);
        }
        printf("\n");
    }
    printf("\nSTOPIDS\n");
    for (i = 0; i < td->n_stops; i++) {
        printf("stop %03d has id %s \n", i, tdata_stop_name_for_index(td, i));
    }
    for (i = 0; i < td->n_routes; i++) {
        /* TODO: Remove?
         * printf("route %03d has id %s and first trip id %s \n", i,
         *        tdata_route_desc_for_index(td, i),
         *        tdata_trip_ids_for_route(td, i));
         */
        tdata_dump_route(td, i, NONE);
    }
}
#endif
