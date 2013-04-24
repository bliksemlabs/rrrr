/* router.c : the main routing algorithm */

#include "router.h"
#include "util.h"
#include "config.h"
#include "qstring.h"
#include "transitdata.h"
#include "bitset.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define WALK -2

void router_setup(router_t *router, transit_data_t *td) {
    srand(time(NULL));
    router->tdata = *td;
    router->table_size = td->nstops * RRRR_MAX_ROUNDS;
    router->best_time = malloc(sizeof(rtime_t) * td->nstops); 
    router->states = malloc(sizeof(router_state_t) * router->table_size);
    router->updated_stops = bitset_new(td->nstops);
    router->updated_routes = bitset_new(td->nroutes);
    if ( ! (router->best_time && router->states && router->updated_stops && router->updated_routes))
        die("failed to allocate router scratch space");
}

static inline void router_reset(router_t router) {

}

void router_teardown(router_t *router) {
    free(router->best_time);
    free(router->states);
    bitset_destroy(router->updated_stops);
    bitset_destroy(router->updated_routes);
}

// flag_routes_for_stops... all at once after doing transfers?
/* Given a stop index, mark all routes that serve it as updated. */
static inline void flag_routes_for_stop (router_t *r, int stop_index, uint32_t date_mask) {
    /*restrict*/ int *routes;
    int n_routes = transit_data_routes_for_stop (&(r->tdata), stop_index, &routes);
    for (int i = 0; i < n_routes; ++i) {
        uint32_t route_active_flags = r->tdata.route_active[routes[i]];
        T printf ("(flagging route %d at stop %d)\n", routes[i], stop_index);
        // CHECK that there are any trips running on this route (another bitfield)
        // printf("route flags %d", route_active_flags);
        // printBits(4, &route_active_flags);
        if (date_mask & route_active_flags) // seems to provide about 14% increase in throughput
           bitset_set (r->updated_routes, routes[i]);
    }
}

/* 
 For each updated stop and each destination of a transfer from an updated stop, 
 set the associated routes as updated. The routes bitset is cleared before the operation, 
 and the stops bitset is cleared after all transfers have been computed and all routes have been set.
 */
static inline void apply_transfers (router_t r, int round, float speed_meters_sec, uint32_t date_mask) {
    transit_data_t d = r.tdata; // this is copying... 
    int nstops = d.nstops;
    router_state_t *states = r.states + (round * nstops);
    bitset_reset (r.updated_routes);
    for (int stop_index_from = bitset_next_set_bit (r.updated_stops, 0); stop_index_from >= 0;
             stop_index_from = bitset_next_set_bit (r.updated_stops, stop_index_from + 1)) {
        D printf ("stop %d was marked as updated \n", stop_index_from);
        flag_routes_for_stop (&r, stop_index_from, date_mask);
        router_state_t *state_from = states + stop_index_from;
        rtime_t time_from = r.best_time[stop_index_from];
        if (time_from != UNREACHED) {
            D printf ("  applying transfer at %d (%s) \n", 
                      stop_index_from, transit_data_stop_id_for_index(&d, stop_index_from));
            /* change to begin, length rather than begin, end in order to restrict pointers ? */
            transfer_t *tr     = d.transfers + d.stops[stop_index_from    ].transfers_offset;
            transfer_t *tr_end = d.transfers + d.stops[stop_index_from + 1].transfers_offset;
            // int n_transfers_for_stop = tr_end - tr;
            for ( ; tr < tr_end ; ++tr) {
                int stop_index_to = tr->target_stop;
                rtime_t time_to = time_from + 
                    (((int)(tr->dist_meters / speed_meters_sec + RRRR_WALK_SLACK_SEC)) >> 1); // to 2-sec resolution
                if (time_to < time_from) {
                    printf ("\ntransfer overflow: %d %d\n", time_from, time_to);
                    continue;
                }
                router_state_t *state_to = states + stop_index_to;
                D printf("    target %d (%s) \n", 
                         stop_index_to, transit_data_stop_id_for_index(&d, stop_index_to));
                if (r.best_time[stop_index_to] > time_to) {
                    D printf ("      setting %d %d\n", stop_index_to, time_to);
                    state_to->arrival_time = time_to;
                    r.best_time[stop_index_to] = time_to;
                    state_to->back_route = WALK; // need sym const for walk distinct from NONE
                    state_to->back_stop = stop_index_from;
                    state_to->back_trip_id = "walk;walk"; // semicolon to provide headsign field in demo
                    state_to->board_time = state_from->arrival_time;
                    flag_routes_for_stop (&r, stop_index_to, date_mask);
                }
            } 
        }
    }
    bitset_reset (r.updated_stops);
}


static void dump_results(router_t *prouter) {
    router_t r = *prouter;
    printf("STOP ARRIVAL\n");
    for (int s = 0; s < r.tdata.nstops; ++s) {
        router_state_t *state = r.states + s;
        bool set = false;
        for (int round = 0; round < RRRR_MAX_ROUNDS; ++round, state += r.tdata.nstops) {
            if (state->arrival_time < UNREACHED) {
                set = true;
                break;
            } 
        }
        if (! set)
            continue;
        char *stop_id = transit_data_stop_id_for_index (&(r.tdata), s);
        printf("%6s ", stop_id);
        for (int round = 0; round < RRRR_MAX_ROUNDS; ++round, state += r.tdata.nstops) {
            printf("%8s ", timetext(state->arrival_time));
        }
        printf("\n");
    }
}

void dump_trips(router_t *prouter) {
    router_t router = *prouter;
    int nroutes = router.tdata.nroutes;
    int tid_width = router.tdata.trip_id_width;
    for (int route = 0; route < nroutes; ++route) {
        char *trip_ids = transit_data_trip_ids_for_route_index(&(router.tdata), route);
        char *trip_ids_end = transit_data_trip_ids_for_route_index(&(router.tdata), route + 1);
        uint32_t *trip_masks = transit_data_trip_masks_for_route_index(&(router.tdata), route);
        uint32_t *trip_masks_end = transit_data_trip_masks_for_route_index(&(router.tdata), route + 1);
        long n_tids = (trip_ids_end - trip_ids) / tid_width;
        long n_masks = trip_masks_end - trip_masks;
        printf ("route %d / %d, n trip_ids %ld, n trip masks %ld\n", route + 1, nroutes, n_tids, n_masks);
        for (int m = 0; m < n_masks; ++m) {
            printf ("trip_id %s mask ", trip_ids + tid_width * m);
            printBits (4, & (trip_masks[m]));
            printf ("\n");
        }
    }
    exit(0);

}

bool router_route(router_t *prouter, router_request_t *preq) {
    router_t router = *prouter; // why copy? consider changing though router contains mostly pointers.
    router_request_t req = *preq;
    //router_request_dump(prouter, preq);

    int nstops = router.tdata.nstops;
    int trip_id_width = router.tdata.trip_id_width;
    uint32_t date_mask = 1 << 3; // as a demo, search on the 3rd day of the schedule
    
    // set initial state
    router_state_t *states = router.states;
    for (int s = 0; s < router.table_size; ++s)
        states[s].arrival_time = UNREACHED;
    for (int s = 0; s < nstops; ++s)
        router.best_time[s] = UNREACHED;
    rtime_t start_time = req.time >> 1;
    D printf("\nstart_time %s \n", timetext(start_time));
    router.best_time[req.from] = start_time;
    states[req.from].arrival_time = start_time;
    bitset_reset(router.updated_stops);
    bitset_set(router.updated_stops, req.from);
    // apply transfers to initial state, which also initializes updated routes bitset
    apply_transfers(router, 0, req.walk_speed, date_mask);
    //dump_results(prouter);
    router_state_t *states_last = states;
    for (int round = 0; round < RRRR_MAX_ROUNDS; ++round, states_last = states) {
        D printf("round %d\n", round);
        states = router.states + round * nstops;
        for (int route = bitset_next_set_bit (router.updated_routes, 0); route >= 0;
                 route = bitset_next_set_bit (router.updated_routes, route + 1)) {
            D printf("  route %d\n", route);
            T transit_data_dump_route(&(router.tdata), route);
            char *route_id = transit_data_route_id_for_index(&(router.tdata), route); // actually contains a detailed route description, not an ID for demo
            /* TODO: restrict pointers ? */ 
            int *s_end;    // pointer one element beyond the end of array
            int *s = transit_data_stops_for_route(router.tdata, route, &s_end);
            int route_nstops = s_end - s;
            rtime_t *t_end;
            rtime_t *t = transit_data_stoptimes_for_route(router.tdata, route, &t_end);
            // rtime_t (*route_stop_times)[route_nstops] = t;
            char *trip_ids = transit_data_trip_ids_for_route_index(&(router.tdata), route); 
            uint32_t *trip_masks = transit_data_trip_masks_for_route_index(&(router.tdata), route); 
            char *trip_id = NULL;
            int trip_index = NONE; // trip index within the route
            int board_stop = NONE; // stop index where that trip was boarded
            int board_time = NONE; // time when that trip was boarded
            for ( ; s < s_end ; ++s, ++t) {
                int stop = *s;
                /* TODO: switch to CamelCase convention for OO classes */
                router_state_t *state = states + stop;
                router_state_t *last_state = states_last + stop;
                T printf("    stop %d %s\n", stop, timetext(router.best_time[stop]));
                /* if we have not yet boarded a trip on this route, see if we can board one */
                // also handle the case where we hit a better arrival time (this is "more efficient" 
                // if we scan backward but scanning forward seems reasonably performant)
                if (trip_index == NONE || last_state->arrival_time + RRRR_XFER_SLACK_2SEC < *t) { 
                    if (router.best_time[stop] == UNREACHED)        
                        continue; 
                    if (last_state->arrival_time == UNREACHED) {
                        // boarding at a place that was not reached in the last round!
                        continue;
                    }
                    // Scan forward to the latest trip that can be boarded, if any.
                    T printf("hit previously-reached stop %d\n", stop);
                    T transit_data_dump_route(&(router.tdata), route);
                    int trip_next = 0;
                    rtime_t *t_next = t;
                    while (t_next < t_end) {
                        T printf("    board option %d at %s \n", trip_next, timetext(*t_next));
                        T printBits(4, & (trip_masks[trip_next]));
                        T printBits(4, & date_mask);
                        T printf("\n");
                        // if this time is later than the best known for this stop
                        if ((*t_next >= router.best_time[stop] + RRRR_XFER_SLACK_2SEC) &&  
                            (date_mask & trip_masks[trip_next])) {  // and if this trip is running
                            T printf("    final option %d at %s\n", trip_next, timetext(*t_next));
                            t = t_next;
                            trip_index = trip_next;
                            trip_id = trip_ids + trip_id_width * trip_index;
                            board_stop = stop;
                            board_time = *t_next;
                            // printf ("boarded %s with mask ", trip_id);
                            // printBits (4, & (trip_masks[trip_next]));
                            // printf ("\n");
                            break;
                        }
                        trip_next += 1;
                        t_next += route_nstops;
                    }
                    if (t_next > t_end) { // overshot the end of the list
                        T printf("    no suitable trip to board.\n");
                    }
                    continue;
                } else if (*t < router.best_time[stop]) { // will also hit UNREACHED (UINT16_MAX)
                    /* We have already boarded a trip along this route. 
                       Check if this trip provides improved arrival times.
                       Only update if the new time is better than the best known,
                       remembering that n_transfers is increasing with each round,
                       and only Pareto-optimal paths count. */
                    state->arrival_time = *t;
                    router.best_time[stop] = *t;
                    state->back_route = route; 
                    state->back_trip_id = route_id; // changed for demo, was: trip_id; 
                    state->back_stop = board_stop;
                    state->board_time = board_time;
                    if (*t < router.best_time[req.to]) { // "target pruning" sec. 3.1
                        bitset_set(router.updated_stops, stop); // mark stop for next round.
                    }
                    T printf("set stop %d to %s\n", stop, timetext(*t));
                } 
            } // end for (stop)
        } // end for (route)
        //if (round < RRRR_MAX_ROUNDS - 1) 
        apply_transfers(router, round, req.walk_speed, date_mask);
        states[req.from].arrival_time = start_time;
        states[req.from].back_stop = -1;
        states[req.from].back_route = -1;
        // exit(0);
    } // end for (round)
    // dump_results(prouter); // DEBUG
    return true;
}

int router_result_dump(router_t *prouter, router_request_t *preq, char *buf, int buflen) {
    router_t router = *prouter;
    router_request_t req = *preq;
    char *b = buf;
    char *b_end = buf + buflen;
    for (int round_outer = 0; round_outer < RRRR_MAX_ROUNDS; ++round_outer) {
        int s = req.to;
        router_state_t *states = router.states + router.tdata.nstops * round_outer;
        if (states[s].arrival_time == UNREACHED)
            continue;
        b += sprintf (b, "\nA %d VEHICLES \n", round_outer + 1);
        int round = round_outer;
        while (round >= 0) {
            states = router.states + router.tdata.nstops * round;
            if (states[s].arrival_time == UNREACHED) {
                round -= 1;
                b += sprintf (b, "%d UNREACHED \n", s);
                continue;
            } 
            //b += sprintf (b, "round %d  ", round);
            int last_stop = states[s].back_stop;
            if (s < 0 || s > router.tdata.nstops) { 
                // this was causing segfaults
                b += sprintf (b, "neg stopid %d\n", s);
                break;
            }
            int route = states[s].back_route;
            char *last_stop_id = transit_data_stop_id_for_index(&(router.tdata), last_stop);
            char *this_stop_id = transit_data_stop_id_for_index(&(router.tdata), s);
            int board  = states[s].board_time;
            int alight = states[s].arrival_time;
            char cboard[255];
            char calight[255];
            btimetext(board, cboard, 255);
            btimetext(alight, calight, 255);
            char *trip_id = states[s].back_trip_id;

            b += sprintf (b, "%s;%s;%s;%s;%s\n", trip_id, 
                last_stop_id, cboard, this_stop_id, calight);
            if (b > b_end) {
                printf ("buffer overflow\n");
                break;
            }
            if (last_stop == req.from) 
                break;
            if (route >= 0 && round > 0)
                round -= 1;
            s = last_stop;
        }
    }
    *b = '\0';
    return b - buf;
}

inline static void set_defaults(router_request_t *req) {
    req->walk_speed = 1.3; // m/sec
    req->from = req->to = req->time = NONE; 
    req->arrive_by = false;
}

int rrrrandom(int limit) {
    return (int) (limit * (random() / (RAND_MAX + 1.0)));
}

void router_request_randomize(router_request_t *req) {
    req->walk_speed = 1.5; // m/sec
    req->from = rrrrandom(5400);
    req->to = rrrrandom(5400);
    req->time = 3600 * 7 + rrrrandom(3600 * 6);
    req->arrive_by = false;
}

inline static bool range_check(router_request_t *req) {
    if (req->walk_speed < 0.1) return false;
    if (req->from < 0) return false;
    if (req->to < 0) return false;
    if (req->time < 0) return false;
    return true;
}

#define BUFLEN 255
bool router_request_from_qstring(router_request_t *req) {
    char *qstring = getenv("QUERY_STRING");
    if (qstring == NULL) 
        qstring = "";
    set_defaults(req);
    char key[BUFLEN];
    char *val;
    while (qstring_next_pair(qstring, key, &val, BUFLEN)) {
        if (strcmp(key, "time") == 0) {
            req->time = atoi(val);
        } else if (strcmp(key, "from") == 0) {
            req->from = atoi(val);
        } else if (strcmp(key, "to") == 0) {
            req->to = atoi(val);
        } else if (strcmp(key, "speed") == 0) {
            req->walk_speed = atof(val);
        } else if (strcmp(key, "randomize") == 0) {
            printf("RANDOMIZING\n");
            router_request_randomize(req);
            return true;
        } else {
            printf("unrecognized parameter: key=%s val=%s\n", key, val);
        }
    }
    return true;
}

void router_request_dump(router_t *router, router_request_t *req) {
    char *from_stop_id = transit_data_stop_id_for_index(&(router->tdata), req->from);
    char *to_stop_id = transit_data_stop_id_for_index(&(router->tdata), req->to);
    printf("from: %s [%d]\n"
           "to:   %s [%d]\n"
           "time: %s [%ld]\n"
           "speed: %f\n", from_stop_id, req->from, to_stop_id, req->to, 
                timetext(req->time >> 1), req->time, req->walk_speed);
}

