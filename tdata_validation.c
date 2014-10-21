/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "tdata_validation.h"
#include "tdata.h"

#include <stdio.h>

/* Validate that the first routestop have won't alighting set and
 * the last routestop won't allow boarding.
 */
int tdata_validation_boarding_alighting(tdata_t *tdata) {
    int32_t ret_invalid = 0;
    uint32_t i_route;

    for (i_route = 0; i_route < tdata->n_routes; ++i_route) {
        route_t *route = &(tdata->routes[i_route]);
        uint8_t *rsa = tdata->route_stop_attributes +
                       route->route_stops_offset;

        if ((rsa[0] & rsa_alighting) == rsa_alighting ||
            (rsa[route->n_stops - 1] & rsa_boarding) == rsa_boarding) {
            fprintf(stderr, "Route index %d %s %s %s has:\n%s%s", i_route,
              tdata_agency_name_for_route(tdata, i_route),
              tdata_shortname_for_route(tdata, i_route),
              tdata_headsign_for_route(tdata, i_route),
              ((rsa[0] & rsa_alighting) == rsa_alighting ?
                "  alighting on the first stop\n" : ""),

              ((rsa[route->n_stops - 1] & rsa_boarding) == rsa_boarding ?
                "  boarding on the last stop\n" : ""));

            ret_invalid--;
        }

        if (ret_invalid < -10) {
            fprintf(stderr, "Too many boarding/alighting problems.\n");
            break;
        }
    }

    return ret_invalid;
}

/* Check that all lat/lon look like valid coordinates.
 * If validation fails the number of invalid coordinates are return
 * as negative value.
 */
int tdata_validation_coordinates(tdata_t *tdata) {

    /* farther south than Ushuaia, Argentina */
    float min_lat = -55.0;

    /* farther north than Tromsø and Murmansk */
    float max_lat = +70.0;
    float min_lon = -180.0;
    float max_lon = +180.0;

    int32_t ret_invalid = 0;

    uint32_t stop_index;
    for (stop_index = 0; stop_index < tdata->n_stops; ++stop_index) {
        latlon_t ll = tdata->stop_coords[stop_index];
        if (ll.lat < min_lat || ll.lat > max_lat ||
            ll.lon < min_lon || ll.lon > max_lon) {
            printf ("stop lat/lon out of range: lat=%f, lon=%f \n",
                                                ll.lat, ll.lon);
            ret_invalid--;
        }
    }

    return ret_invalid;
}

/* Check that all timedemand types start at 0 and consist of
 * monotonically increasing times.
 */
int tdata_validation_increasing_times(tdata_t *tdata) {

    uint32_t route_index, stop_index, trip_index;
    int ret_nonincreasing = 0;

    for (route_index = 0; route_index < tdata->n_routes; ++route_index) {
        route_t route = tdata->routes[route_index];
        trip_t *trips = tdata->trips + route.trip_ids_offset;
        int n_nonincreasing_trips = 0;
        for (trip_index = 0; trip_index < route.n_trips; ++trip_index) {
            trip_t trip = trips[trip_index];
            stoptime_t *prev_st = NULL;
            for (stop_index = 0; stop_index < route.n_stops; ++stop_index) {
                stoptime_t *st = tdata->stop_times + trip.stop_times_offset
                                                   + stop_index;
                if (stop_index == 0 && st->arrival != 0) fprintf (stderr, "timedemand type begins at %d,%d not 0.\n", st->arrival, st->departure);
                if (st->departure < st->arrival) fprintf (stderr, "departure before arrival at route %d, trip %d, stop %d.\n", route_index, trip_index, stop_index);

                if (prev_st != NULL) {
                    if (st->arrival < prev_st->departure) {
                        #if 0
                        fprintf (stderr, "negative travel time arriving at route %d, trip %d, stop %d.\n", r, t, s);
                        fprintf (stderr, "(%d, %d) -> (%d, %d)\n", prev_st->arrival, prev_st->departure, st->arrival, st->departure);
                        #endif
                        n_nonincreasing_trips += 1;
                    } /* there are also lots of 0 travel times... */
                }
                prev_st = st;
            }
        }
        if (n_nonincreasing_trips > 0) {
            fprintf (stderr, "route %d has %d trips with negative travel times\n",
                             route_index, n_nonincreasing_trips);
            ret_nonincreasing -= n_nonincreasing_trips;
        }
    }

    return ret_nonincreasing;
}

/* Check that all transfers are symmetric.
 */
int tdata_validation_symmetric_transfers(tdata_t *tdata) {
    int n_transfers_checked = 0;
    uint32_t stop_index_from;
    for (stop_index_from = 0; stop_index_from < tdata->n_stops; ++stop_index_from) {

        /* Iterate over all transfers going out of this stop */
        uint32_t t  = tdata->stops[stop_index_from    ].transfers_offset;
        uint32_t tN = tdata->stops[stop_index_from + 1].transfers_offset;
        for ( ; t < tN ; ++t) {
            uint32_t stop_index_to = tdata->transfer_target_stops[t];
            uint32_t forward_distance = tdata->transfer_dist_meters[t] << 4;
            /*                          actually in units of 2^4 == 16 meters */

            /* Find the reverse transfer (stop_index_to -> stop_index_from) */
            uint32_t u  = tdata->stops[stop_index_to    ].transfers_offset;
            uint32_t uN = tdata->stops[stop_index_to + 1].transfers_offset;
            bool found_reverse = false;

            if (stop_index_to == stop_index_from) fprintf (stderr, "loop transfer from/to stop %d.\n", stop_index_from);

            for ( ; u < uN ; ++u) {
                n_transfers_checked += 1;
                if (tdata->transfer_target_stops[u] == stop_index_from) {
                    /* this is the same transfer in reverse */
                    uint32_t reverse_distance = tdata->transfer_dist_meters[u] << 4;
                    if (reverse_distance != forward_distance) {
                        fprintf (stderr, "transfer from %d to %d is not symmetric. "
                                         "forward distance is %d, reverse distance is %d.\n",
                                         stop_index_from, stop_index_to, forward_distance, reverse_distance);
                    }
                    found_reverse = true;
                    break;
                }
            }
            if ( ! found_reverse) {
                fprintf (stderr, "transfer from %d to %d does not have an equivalent reverse transfer.\n", stop_index_from, stop_index_to);
                return -1;
            }
        }
    }
    fprintf (stderr, "checked %d transfers for symmetry.\n", n_transfers_checked);

    return 0;
}

bool tdata_validation_check_coherent (tdata_t *tdata) {
    fprintf (stderr, "checking tdata coherency...\n");

    return  (tdata_validation_boarding_alighting(tdata) == 0 &&
             tdata_validation_coordinates(tdata) == 0 &&
             tdata_validation_increasing_times(tdata) == 0 &&
             tdata_validation_symmetric_transfers(tdata) == 0);
}

