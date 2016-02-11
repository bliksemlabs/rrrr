#include "config.h"
#include "expand_vehicle_journeys.h"
#include "plan_render_text.h"
#include <stdlib.h>
#include <string.h>

/* Helper function to fill an existing connection using tdata_t types */
static void
add_connection(const tdata_t *td, vehicle_journey_t *vjs, stoptime_t *timedemand_type, spidx_t *stops,
               jpidx_t i_jp, jp_vjoffset_t i_vj, jppidx_t i_jpp, rtime_t translate,
               vjidx_t vj_index, connection_t *connection) {

    connection->departure = translate + vjs[i_vj].begin_time + timedemand_type[i_jpp].departure - td->stop_point_waittime[stops[i_jpp]];
    connection->arrival   = translate + vjs[i_vj].begin_time + timedemand_type[i_jpp + 1].arrival;
    connection->sp_from   = stops[i_jpp];
    connection->sp_to     = stops[i_jpp + 1];
#ifdef METADATA
    connection->journey_pattern = i_jp;
    connection->vehicle_journey = i_vj;
#endif
    connection->vj_index = vj_index;
}

/* Calculate the number of connections per day */
static bool
calculate_connections (const tdata_t *td, router_request_t *req,
                       conidx_t *n_connections) {
    jpidx_t i_jp;
    calendar_t yesterday_mask = req->day_mask >> 1;
    calendar_t today_mask     = req->day_mask;
    calendar_t tomorrow_mask  = req->day_mask << 1;

    conidx_t i_connections = 0;

    i_jp = (jpidx_t) td->n_journey_patterns;
    while (i_jp) {
        jp_vjoffset_t i_vj;
        i_jp--;
        i_vj = td->journey_patterns[i_jp].n_vjs;

        calendar_t *vj_masks = tdata_vj_masks_for_journey_pattern(td, i_jp);
        jppidx_t i_jpp = td->journey_patterns[i_jp].n_stops - 1;


        while (i_vj) {
            i_vj--;
            /*
            if (vj_masks[i_vj] & yesterday_mask) {
                i_connections += i_jpp;
            }*/
            if (vj_masks[i_vj] & today_mask) {
                i_connections += i_jpp;
            }
            /*
            if (vj_masks[i_vj] & tomorrow_mask) {
                i_connections += i_jpp;
            }*/
        }
    }

    *n_connections = i_connections;

    return true;
}

/* Expand all trips into connections */
bool expand_vehicle_journeys (const tdata_t *td, router_request_t *req,
                              connection_t *connections, conidx_t n_connections) {
    jpidx_t i_jp;
    calendar_t yesterday_mask = req->day_mask >> 1;
    calendar_t today_mask     = req->day_mask;
    calendar_t tomorrow_mask  = req->day_mask << 1;

    i_jp = (jpidx_t) td->n_journey_patterns;
    while (i_jp) {
        jp_vjoffset_t i_vj;
        i_jp--;
        i_vj = td->journey_patterns[i_jp].n_vjs;

        calendar_t *vj_masks = tdata_vj_masks_for_journey_pattern(td, i_jp);
        vehicle_journey_t *vjs = tdata_vehicle_journeys_in_journey_pattern(td, i_jp);
        spidx_t *stops = tdata_points_for_journey_pattern(td, i_jp);
        jppidx_t n_stops = td->journey_patterns[i_jp].n_stops - 1;

        while (i_vj) {
            stoptime_t *timedemand_type;
            vjidx_t vj_index;
            jppidx_t i_jpp;

            i_vj--;
            timedemand_type = tdata_timedemand_type(td, i_jp, i_vj);
            vj_index = td->journey_patterns[i_jp].vj_index + i_vj;
            /*
            if (vj_masks[i_vj] & yesterday_mask) {
                i_jpp = n_stops;
                while (i_jpp) {
                    n_connections--;
                    i_jpp--;
                    add_connection(td, vjs, timedemand_type, stops, i_jp, i_vj, i_jpp, 0,              vj_index, &connections[n_connections]);
                }
            }*/

            if (vj_masks[i_vj] & today_mask) {
                i_jpp = n_stops;
                while (i_jpp) {
                    n_connections--;
                    i_jpp--;
                    add_connection(td, vjs, timedemand_type, stops, i_jp, i_vj, i_jpp, RTIME_ONE_DAY,  vj_index, &connections[n_connections]);
                }
            }
            /*
            if (vj_masks[i_vj] & tomorrow_mask) {
                i_jpp = n_stops;
                while (i_jpp) {
                    n_connections--;
                    i_jpp--;
                    add_connection(td, vjs, timedemand_type, stops, i_jp, i_vj, i_jpp, RTIME_TWO_DAYS, vj_index, &connections[n_connections]);
                }
            }*/
        }
    }

    return (n_connections == 0);
}

/* Helper function for debugging */
void dump_connection(const tdata_t *td, connection_t *connection) {
    char departure[13], arrival[13];
    const char *sp_from = tdata_stop_point_name_for_index(td, connection->sp_from);
    const char *sp_to   = tdata_stop_point_name_for_index(td, connection->sp_to);
    btimetext(connection->departure, departure);
    btimetext(connection->arrival,   arrival);

    printf("%.6u %s %s %.6u %.25s | %.25s %.6u %.5u %.3u\n", connection->vj_index, departure, arrival, connection->departure, sp_from, sp_to, connection->arrival, connection->journey_pattern, connection->vehicle_journey);
}

/* Dump all connections */
void dump_connections(const tdata_t *td, connection_t *connections, conidx_t n_connections) {
    conidx_t i;
    for (i = 0; i < n_connections; ++i) {
        dump_connection(td, &connections[i]);
    }
}

/* Router intialisation */
bool csa_router_setup (csa_router_t *router, tdata_t *tdata) {
    router->tdata = tdata;
    router->best_time = (rtime_t *) malloc (sizeof(rtime_t) * tdata->n_stop_points);
    router->states_back_connection = (conidx_t *) malloc (sizeof(conidx_t) * tdata->n_stop_points);
    if ( ! (router->best_time
            && router->states_back_connection
           )
       ) {
        fprintf(stderr, "failed to allocate router scratch space\n");
        return false;
    }

    return true;
}

/* Sort two connections by ascending departure and ascending arrival time */
static int
compare_connection_departure(const void *elem1, const void *elem2) {
    const connection_t *i1 = (const connection_t *) elem1;
    const connection_t *i2 = (const connection_t *) elem2;

    return ((i1->departure - i2->departure) << 16) +
             i1->arrival   - i2->arrival;
}

/* Sort two connections by descending arrival and descending departure time */
static int
compare_connection_arrival(const void *elem1, const void *elem2) {
    const connection_t *i1 = (const connection_t *) elem1;
    const connection_t *i2 = (const connection_t *) elem2;

    return ((i2->arrival   - i1->arrival) << 16) +
             i2->departure - i1->departure;
}

/* Deallocate the scratch space */
void csa_router_teardown (csa_router_t *router) {
    free (router->best_time);
    free (router->states_back_connection);
}

/* 1. This function computes the required allocation for the connections.
 * 2. Then allocates both departure and arrival series.
 * 3. Computes the connections.
 * 4. Sorts for the departure series.
 * 5. Copies the sorted series for arrivals.
 * 6. Sorts for the arrival series.
 */
bool csa_router_setup_connections (csa_router_t *router, router_request_t *req) {
    calculate_connections (router->tdata, req, &router->n_connections);
    router->connections_departure = (connection_t *) malloc(sizeof(connection_t) * router->n_connections);
    router->connections_arrival   = (connection_t *) malloc(sizeof(connection_t) * router->n_connections);
    router->onboard = bitset_new (router->tdata->n_vjs);

    if ( ! (router->connections_departure
            && router->connections_arrival
            && router->onboard)
       ) {
        fprintf(stderr, "failed to allocate router connections\n");
        return false;
    }

    if (expand_vehicle_journeys (router->tdata, req, router->connections_departure, router->n_connections)) {
        qsort (router->connections_departure, router->n_connections, sizeof(connection_t), compare_connection_departure);
        memcpy (router->connections_arrival, router->connections_departure, sizeof(connection_t) * router->n_connections);
        qsort (router->connections_arrival, router->n_connections, sizeof(connection_t), compare_connection_arrival);

        return true;
    }

    return false;
}

/* We envision the connections eventually as some sort of shared structure
 * similar how tdata_t * is working.
 */
void csa_router_teardown_connections (csa_router_t *router) {
    free (router->connections_departure);
    free (router->connections_arrival);
}

/* Transfer on foot */
static void csa_transfer (csa_router_t *router, router_request_t *req,
                          conidx_t i_con, spidx_t sp_index_from, rtime_t time_from) {
    uint32_t tr     = router->tdata->stop_points[sp_index_from].transfers_offset;
    uint32_t tr_end = router->tdata->stop_points[sp_index_from + 1].transfers_offset;

    for ( ; tr < tr_end ; ++tr) {
        /* Transfer durations are stored in r_time */
        spidx_t sp_index_to = router->tdata->transfer_target_stops[tr];
        rtime_t transfer_duration = router->tdata->transfer_durations[tr] + req->walk_slack;
        rtime_t time_to = req->arrive_by ? time_from - transfer_duration
                                         : time_from + transfer_duration;

        if ((req->arrive_by ? time_to > router->best_time[sp_index_to]
                            : time_to < router->best_time[sp_index_to])) {

            router->best_time[sp_index_to] = time_to;
            router->states_back_connection[sp_index_to] = i_con;

            if (sp_index_to == req->to_stop_point) {
                req->time_cutoff = MIN(req->time_cutoff, time_to);
            }
        }
    }
}

/* Implements an ordinary bsearch, which guarantees an underfitted needle */
static conidx_t
csa_binary_search_departure (csa_router_t *router, router_request_t *req) {
    conidx_t low, mid, high;
    low = 0;
    high = router->n_connections - 1;

    do {
        mid = (low + high) >> 1;
        if (req->time < router->connections_departure[mid].departure) {
            high = mid - 1;
        } else if (req->time > router->connections_departure[mid].departure) {
            low = mid + 1;
        }
    } while (req->time != router->connections_departure[mid].departure && low <= high);

    fprintf(stderr, "start vanaf %u\n", low);

    return low;
}

/* Board all previous trips in the same block */
void csa_board_forward (csa_router_t *router, vjidx_t vj_index) {
    vehicle_journey_ref_t *interline;

    bitset_set (router->onboard, vj_index);

    interline = &router->tdata->vehicle_journey_transfers_forward [vj_index];

    while (interline->jp_index != JP_NONE) {
        vj_index = router->tdata->journey_patterns[interline->jp_index].vj_index + interline->vj_offset;

        bitset_set (router->onboard, vj_index);

        interline = &router->tdata->vehicle_journey_transfers_forward [vj_index];
    }
}

/* Implements ordinary, single criteria CSA */
bool csa_router_route_departure (csa_router_t *router, router_request_t *req) {
    conidx_t i_con;
    /* initialize_origin (router, req); */

    bitset_clear (router->onboard);
    rrrr_memset (router->states_back_connection, CON_NONE, router->tdata->n_stop_points);
    rrrr_memset (router->best_time, UNREACHED, router->tdata->n_stop_points);
    router->best_time[req->from_stop_point] = req->time;

    i_con = csa_binary_search_departure (router, req);

    for (; i_con < router->n_connections; ++i_con) {
        connection_t *con = &router->connections_departure[i_con];
        bool onboard  = bitset_get (router->onboard, con->vj_index);
        bool improves = con->arrival < router->best_time[con->sp_to];

        if ((onboard || con->departure >= router->best_time[con->sp_from]) &&
             improves) {
            if (!onboard) csa_board_forward (router, con->vj_index);
            router->best_time[con->sp_to] = con->arrival;
            router->states_back_connection[con->sp_to] = i_con;

            csa_transfer (router, req, i_con, con->sp_to, con->arrival);

            if (con->sp_to == req->to_stop_point) {
                req->time_cutoff = MIN(req->time_cutoff, con->arrival);
            }
        } else if (con->arrival > req->time_cutoff) {
            break;
        }
    }

    return req->to_stop_point == STOP_NONE || (router->states_back_connection[req->to_stop_point] != CON_NONE);
}

/* Implements a bsearch on a reverse sorted list, which guarantees an underfitted needle */
static conidx_t
csa_binary_search_arrival (csa_router_t *router, router_request_t *req) {
    conidx_t low, mid, high;
    low = 0;
    high = router->n_connections - 1;

    do {
        mid = (low + high) >> 1;
        if (req->time > router->connections_arrival[mid].arrival) {
            high = mid - 1;
        } else if (req->time < router->connections_arrival[mid].arrival) {
            low = mid + 1;
        }
    } while (req->time != router->connections_arrival[mid].arrival && low <= high);

    return low;
}

/* Board all upcoming trips in the same block */
void csa_board_backward (csa_router_t *router, vjidx_t vj_index) {
    vehicle_journey_ref_t *interline;

    bitset_set (router->onboard, vj_index);

    interline = &router->tdata->vehicle_journey_transfers_backward[vj_index];

    while (interline->jp_index != JP_NONE) {
        vj_index = router->tdata->journey_patterns[interline->jp_index].vj_index + interline->vj_offset;

        bitset_set (router->onboard, vj_index);

        interline = &router->tdata->vehicle_journey_transfers_backward[vj_index];
    }
}

/* Implements arrive-by single criteria CSA */
bool csa_router_route_arrival (csa_router_t *router, router_request_t *req) {
    conidx_t i_con;
    /* initialize_destination (router, req); */

    bitset_clear (router->onboard);
    rrrr_memset (router->states_back_connection, CON_NONE, router->tdata->n_stop_points);
    rrrr_memset (router->best_time, 0, router->tdata->n_stop_points);

    street_network_t *origin = &req->exit;
    spidx_t i_sp = origin->n_points;
    while (i_sp) {
        i_sp--;
        router->best_time[origin->stop_points[i_sp]] = req->time;
    }
    /* router->best_time[req->to_stop_point] = req->time; */

    i_con = csa_binary_search_arrival (router, req);

    for (; i_con < router->n_connections; ++i_con) {
        connection_t *con = &router->connections_arrival[i_con];
        bool onboard  = bitset_get (router->onboard, con->vj_index);
        bool improves = con->departure > router->best_time[con->sp_from];

        if ((onboard || con->arrival <= router->best_time[con->sp_to]) &&
             improves) {
            if (!onboard) csa_board_backward (router, con->vj_index);
            router->best_time[con->sp_from] = con->departure;
            router->states_back_connection[con->sp_from] = i_con;

            csa_transfer (router, req, i_con, con->sp_from, con->departure);

            if (con->sp_from == req->from_stop_point) {
                req->time_cutoff = MAX(req->time_cutoff, con->departure);
            }

        /* The following might look strange because intuitively we
         * should compare con->departure instead of con->arrival.
         * Now consider a very long connection. If we would evaluate
         * this departure time, this may well be below the cutoff,
         * while the next connection arrives earlier, but departs
         * later as well.
         *
         *        cutoff
         * 1.       |   o------o
         * 2.     o-|---------o
         * 3.       |   o----o
         * 4.  o---o|
         *        break
         */
        } else if (con->arrival < req->time_cutoff) {
            break;
        }
    }

    return req->from_stop_point == STOP_NONE || (router->states_back_connection[req->from_stop_point] != CON_NONE);
}

/* The chain of connections is traversed backwards,
 * for ordinary searches we must reverse the rendered legs.
 */
static void
reverse_legs (itinerary_t *itin) {
    uint8_t left  = 0;
    uint8_t right = itin->n_legs - 1;
    while (left < right) {
        leg_t tmp = itin->legs[left];
        itin->legs[left++] = itin->legs[right];
        itin->legs[right--] = tmp;
    }
}

/* Render a chain of connections into a plan */
bool csa_router_result_to_plan (plan_t *plan, csa_router_t *router, router_request_t *req) {
    /* allows the function to be abstractly used on both arrive-by and depart-by queries */
    connection_t *connections;

    /* Iterators */
    connection_t *connection = NULL, *prev = NULL;
    conidx_t last_idx = CON_NONE;
    conidx_t prev_idx = CON_NONE;

    /* Setup the plan */
    plan->req = *req;
    plan->n_itineraries = 0;

    /* Setup the first leg */
    itinerary_t *itin = &plan->itineraries[plan->n_itineraries];
    itin->n_legs = 0;
    if (req->arrive_by) {
        connections = router->connections_arrival;
        last_idx = router->states_back_connection[req->from_stop_point];
    } else {
        connections = router->connections_departure;
        last_idx = router->states_back_connection[req->to_stop_point];
    }

    leg_t *leg = &itin->legs[itin->n_legs];
    leg->journey_pattern = JP_NONE;
    leg->vj = VJ_NONE;

    while (last_idx != CON_NONE) {
        connection = &connections[last_idx];

        if (leg->journey_pattern != connection->journey_pattern ||
            leg->vj != connection->vehicle_journey) {
            leg = &itin->legs[itin->n_legs];
            itin->n_legs += 1;
            leg->sp_from         = connection->sp_from;
            leg->t0              = connection->departure + router->tdata->stop_point_waittime[connection->sp_from];
            leg->sp_to           = connection->sp_to;
            leg->t1              = connection->arrival;
            leg->journey_pattern = connection->journey_pattern;
            leg->vj              = connection->vehicle_journey;
        } else {
            if (!req->arrive_by) {
                leg->sp_from = connection->sp_from;
                leg->t0      = connection->departure + router->tdata->stop_point_waittime[connection->sp_from];
            } else {
                leg->sp_to   = connection->sp_to;
                leg->t1      = connection->arrival;
            }
        }

        last_idx = router->states_back_connection[(req->arrive_by ? connection->sp_to : connection->sp_from)];
    }

    if (itin->n_legs != 0) {
        if (!req->arrive_by) {
            reverse_legs(itin);
        }
        itin->n_rides = itin->n_legs;
        plan->n_itineraries += 1;
    } else {
        return false;
    }

    return true;
}

void csa_dump_best_times_departure (const csa_router_t *router, const router_request_t *req) {
    bool open = false;

    puts("{\"type\":\"FeatureCollection\",\"features\":[");

    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;

        if (router->best_time[i_sp] != UNREACHED) {
            latlon_t *latlon = &router->tdata->stop_point_coords[i_sp];
            if (open) puts(",");
            printf("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.4f,%.4f]},\"properties\":{\"arrival\":%u}}", latlon->lon, latlon->lat, (router->best_time[i_sp] - req->time) << 2);
            open = true;
        }
    }

    puts("]}");
}

void csa_dump_best_times_arrival (const csa_router_t *router, const router_request_t *req) {
    bool open = false;

    puts("{\"type\":\"FeatureCollection\",\"features\":[");

    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;

        if (router->best_time[i_sp] != 0) {
            latlon_t *latlon = &router->tdata->stop_point_coords[i_sp];
            if (open) puts(",");
            printf("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.4f,%.4f]},\"properties\":{\"arrival\":%u}}", latlon->lon, latlon->lat, (req->time - router->best_time[i_sp]) << 2);
            open = true;
        }
    }

    puts("]}");
}

void csa_dump_best_times_arrival_sum (const csa_router_t *router, const router_request_t *req, rtime_t *sum) {
    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;
        sum[i_sp] += (req->time - router->best_time[i_sp]);
    }
}

typedef struct sorted_rtime sorted_rtime_t;
struct sorted_rtime {
    rtime_t time;
    spidx_t sp;
};

/* Sort two connections by ascending departure and ascending arrival time */
static int
compare_rtime(const void *elem1, const void *elem2) {
    const sorted_rtime_t *i1 = (const sorted_rtime_t *) elem1;
    const sorted_rtime_t *i2 = (const sorted_rtime_t *) elem2;

    return (i2->time - i1->time);
}

void csa_dump_result_aggr (const csa_router_t *router, rtime_t *sum) {
    sorted_rtime_t *sorted = (sorted_rtime_t *) malloc (sizeof(sorted_rtime_t *) * router->tdata->n_stop_points);

    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;
        sorted[i_sp].time = sum[i_sp];
        sorted[i_sp].sp = i_sp;
    }

    qsort (sorted, router->tdata->n_stop_points, sizeof(sorted_rtime_t), compare_rtime);

    bool open = false;

    puts("{\"type\":\"FeatureCollection\",\"features\":[");

    i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;
        sorted_rtime_t *needle = &sorted[i_sp];
        if (needle->time == UNREACHED) {
            continue;
        }
        latlon_t *latlon = &router->tdata->stop_point_coords[needle->sp];
        if (open) puts(",");
        printf("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.4f,%.4f]},\"properties\":{\"weight\":%u}}", latlon->lon, latlon->lat, needle->time);
        open = true;
    }

    puts("]}");
    free (sorted);
}

void csa_dump_connections_arrival (const csa_router_t *router, const router_request_t *req) {
#if 0
    bitset_t *used = bitset_new (router->n_connections);
    bitset_clear(used);


    connection_t *connections;
    if (req->arrive_by) {
        connections = router->connections_arrival;
    } else {
        connections = router->connections_departure;
    }

    latlon_t *latlon = &router->tdata->stop_point_coords[req->to_stop_point];

    puts("{\"type\":\"FeatureCollection\",\"features\":[");
    printf("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.4f,%.4f]},\"properties\":{\"arrival\":%u}}", latlon->lon, latlon->lat, (req->time - router->best_time[req->to_stop_point]) << 2);

    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;
        conidx_t last_idx = router->states_back_connection[i_sp];

        connection_t *connection = NULL;
        while (last_idx != CON_NONE && !bitset_get(used, last_idx)) {
            if (!connection) puts(",{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[");

            connection = &connections[last_idx];
            bitset_set(used, last_idx);
            latlon = &router->tdata->stop_point_coords[connection->sp_to];
            printf("[%.4f,%.4f],", latlon->lon, latlon->lat);
            last_idx = router->states_back_connection[(req->arrive_by ? connection->sp_to : connection->sp_from)];
        }

        if (connection) {
            latlon = &router->tdata->stop_point_coords[connection->sp_from];
            printf("[%.4f,%.4f]]},\"properties\":{}}", latlon->lon, latlon->lat);
        }
    }

    puts("]}");

    bitset_destroy(used);
#endif
#if 0
    connection_t *connections;
    latlon_t *latlon = &router->tdata->stop_point_coords[req->to_stop_point];

    if (req->arrive_by) {
        connections = router->connections_arrival;
    } else {
        connections = router->connections_departure;
    }

    puts("{\"type\":\"FeatureCollection\",\"features\":[");
    printf("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.4f,%.4f]},\"properties\":{\"arrival\":%u}}", latlon->lon, latlon->lat, (req->time - router->best_time[req->to_stop_point]) << 2);

    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;
        conidx_t last_idx = router->states_back_connection[i_sp];
        puts(",{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[");
        connection_t *connection = NULL;
        while (last_idx != CON_NONE) {
            connection = &connections[last_idx];
            latlon_t *latlon = &router->tdata->stop_point_coords[connection->sp_to];
            printf("[%.4f,%.4f],", latlon->lon, latlon->lat);
            last_idx = router->states_back_connection[(req->arrive_by ? connection->sp_to : connection->sp_from)];
        }

        if (connection) {
            latlon_t *latlon = &router->tdata->stop_point_coords[connection->sp_from];
            printf("[%.4f,%.4f]", latlon->lon, latlon->lat);
        }

        puts("]},\"properties\":{}}");
    }
    puts("]}");
#endif
#if 0
    bitset_t *used = bitset_new (router->n_connections);
    bitset_clear(used);

    if (req->arrive_by) {
        connections = router->connections_arrival;
    } else {
        connections = router->connections_departure;
    }

    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;
        conidx_t last_idx = router->states_back_connection[i_sp];

        while (last_idx != CON_NONE) {
            bool is_used = bitset_get(used, last_idx);
            if (!is_used) {
                connection_t *connection;
                bitset_set(used, last_idx);
                connection = &connections[last_idx];
                last_idx = router->states_back_connection[(req->arrive_by ? connection->sp_to : connection->sp_from)];
            } else {
                /* if the connection is already present, we already have the sink tree down */
                break;
            }
        }
    }

    puts("{\"type\":\"FeatureCollection\",\"features\":[");
    printf("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.4f,%.4f]},\"properties\":{\"arrival\":%u}}", latlon->lon, latlon->lat, (req->time - router->best_time[req->to_stop_point]) << 2);

    conidx_t i_con;
    for (i_con  = bitset_next_set_bit (used, 0);
         i_con != BITSET_NONE;
         i_con  = bitset_next_set_bit (used, i_con + 1)) {
            connection_t *con = &router->connections_arrival[i_con];

            latlon_t *latlon_sp_from = &router->tdata->stop_point_coords[con->sp_from];
            latlon_t *latlon_sp_to   = &router->tdata->stop_point_coords[con->sp_to];


            printf(",{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[%.4f,%.4f],[%.4f,%.4f]]},\"properties\":{\"departure\":%u,\"arrival\":%u}}", latlon_sp_from->lon, latlon_sp_from->lat, latlon_sp_to->lon, latlon_sp_to->lat, RTIME_TO_SEC(req->time - con->departure), RTIME_TO_SEC(req->time - con->arrival));
    }

    puts("]}");

    bitset_destroy(used);
#endif

#if 1
    puts("{\"type\":\"FeatureCollection\",\"features\":[");
    rtime_t best_time = router->best_time[req->to_stop_point]; /* req->time */
    bool open = false;
    spidx_t i_sp = router->tdata->n_stop_points;
    while (i_sp) {
        i_sp--;

        if (router->states_back_connection[i_sp] != CON_NONE) {
            connection_t *con = &router->connections_arrival[router->states_back_connection[i_sp]];

            latlon_t *latlon_sp_from = &router->tdata->stop_point_coords[con->sp_from];
            latlon_t *latlon_sp_to   = &router->tdata->stop_point_coords[con->sp_to];
            if (open) puts(",");
            printf("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[%.4f,%.4f],[%.4f,%.4f]]},\"properties\":{\"departure\":%u,\"arrival\":%u}}", latlon_sp_from->lon, latlon_sp_from->lat, latlon_sp_to->lon, latlon_sp_to->lat, RTIME_TO_SEC(best_time - con->departure), RTIME_TO_SEC(best_time - con->arrival));
            open = true;
        }
    }

    puts("]}");
#endif
}



void csa_scan_all_departure (csa_router_t *router, router_request_t *req) {
    router_request_t scan_req = *req;
    scan_req.to_stop_point = STOP_NONE;
    scan_req.time_cutoff = UNREACHED;
    csa_router_route_departure (router, &scan_req);
    csa_dump_best_times_departure (router, &scan_req);
}

void csa_scan_all_arrival (csa_router_t *router, router_request_t *req) {
    router_request_t scan_req = *req;
    scan_req.from_stop_point = STOP_NONE;
    scan_req.time_cutoff = 0;
    csa_router_route_arrival (router, &scan_req);
    /* csa_dump_connections_arrival (router, &scan_req); */
    /* csa_dump_best_times_arrival (router, &scan_req); */
}

#if 0
int main(int argc, char *argv[]) {
    int status = EXIT_SUCCESS;
    csa_router_t router;
    router_request_t req;
    tdata_t tdata;
    plan_t plan;
    memset (&router, 0, sizeof(csa_router_t));
    memset (&tdata, 0, sizeof(tdata_t));
    memset (&plan, 0, sizeof(plan_t));
    if ( ! tdata_load (&tdata, argv[1])) {
        goto clean_exit;
    }

    router_request_initialize (&req);
    router_request_from_epoch (&req, &tdata, strtoepoch("2015-04-13T12:00:00"));

    if (! tdata_hashgrid_setup (&tdata)) {
        status = EXIT_FAILURE;
        goto clean_exit;
    }

    if ( ! csa_router_setup (&router, &tdata)) {
        /* if the memory is not allocated we must exit */
        status = EXIT_FAILURE;
        goto clean_exit;
    }

    if ( ! csa_router_setup_connections (&router, &req)) {
        status = EXIT_FAILURE;
        goto clean_exit;
    }

    rtime_t *sum = (rtime_t *) malloc (sizeof(rtime_t) * tdata.n_stop_points);
    rrrr_memset (sum, 0, router.tdata->n_stop_points);

    req.arrive_by = true;

    strtolatlon("52.2989,5.6237", &req.from_latlon);
    strtolatlon("52.2989,5.6237", &req.to_latlon);
    router_request_search_street_network (&req, router.tdata);
    csa_scan_all_arrival (&router, &req);
    csa_dump_best_times_arrival_sum (&router, &req, sum);

    strtolatlon("52.089,5.112", &req.from_latlon);
    strtolatlon("52.089,5.112", &req.to_latlon);
    router_request_search_street_network (&req, router.tdata);
    csa_scan_all_arrival (&router, &req);

    csa_dump_best_times_arrival_sum (&router, &req, sum);
    csa_dump_result_aggr (&router, sum);


#if 0
    /* forward search */

    csa_router_route_departure (&router, &req);

    csa_router_result_to_plan (&plan, &router, &req);

    #define OUTPUTLEN 50000
    char result_buf[OUTPUTLEN];
    plan_render_text (&plan, &tdata, result_buf, OUTPUTLEN);
    puts(result_buf);

    /* backward search */
    router_request_t new_req = req;

    new_req.time_cutoff = req.time;
    new_req.time = req.time_cutoff;
    new_req.arrive_by = true;

    char t[13], t2[13];
    printf("cutoff time %u %u %s %s\n", new_req.time_cutoff, new_req.time, btimetext(new_req.time_cutoff, t), btimetext(new_req.time, t2));

    csa_router_route_arrival (&router, &new_req);

    memset (&plan, 0, sizeof(plan_t));
    csa_router_result_to_plan (&plan, &router, &new_req);

    plan_render_text (&plan, &tdata, result_buf, OUTPUTLEN);
    puts(result_buf);

    #ifdef RRRR_DEBUG
    /* dump_connections (&tdata, router.connections_departure, router.n_connections); */
    /* dump_connections (&tdata, router.connections_arrival, router.n_connections); */
    #endif
#endif

clean_exit:
    #ifndef RRRR_VALGRIND
    goto fast_exit;
    #endif

    /* Deallocate the scratchspace of the router */
    csa_router_teardown (&router);

    /* Remove the connections */
    csa_router_teardown_connections (&router);

    /* Deallocate the hashgrid coordinates */
    tdata_hashgrid_teardown (&tdata);

    /* Unmap the memory and/or deallocate the memory on the heap */
    tdata_close (&tdata);

    #ifdef RRRR_VALGRIND
    goto fast_exit; /* kills the unused label warning */
    #endif

fast_exit:
    exit(status);
}
#endif
