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

#define WALK -2

void router_setup(router_t *router, transit_data_t *td) {
    srand(time(NULL));
    router->tdata = *td;
    router->table_size = td->nstops * RRRR_MAX_ROUNDS;
    router->best_time = malloc(sizeof(int) * td->nstops); 
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
}

static inline void flag_routes_for_stop (router_t *r, int stop_index) {
    /*restrict*/ int *routes;
    int n_routes = transit_data_routes_for_stop (&(r->tdata), stop_index, &routes);
    for (int i = 0; i < n_routes; ++i) {
        D printf ("(flagging route %d at stop %d)\n", routes[i], stop_index);
        bitset_set (r->updated_routes, routes[i]);
    }
}

static inline void apply_transfers (router_t r, int round, float speed_meters_sec) {
    transit_data_t d = r.tdata;
    int nstops = d.nstops;
    router_state_t *states = r.states + (round * nstops);
    bitset_reset (r.updated_routes);
    for (int stop_index_from = bitset_next_set_bit (r.updated_stops, 0); stop_index_from >= 0;
             stop_index_from = bitset_next_set_bit (r.updated_stops, stop_index_from + 1)) {
        D printf ("stop %d was marked as updated \n", stop_index_from);
        flag_routes_for_stop (&r, stop_index_from);
        router_state_t *state_from = states + stop_index_from;
        int time_from = r.best_time[stop_index_from];
        if (time_from != INF) {
            D printf ("  applying transfer at %d (%s) \n", 
                      stop_index_from, transit_data_stop_id_for_index(&d, stop_index_from));
            /* change to begin, length rather than begin, end in order to restrict pointers ? */
            transfer_t *tr     = d.transfers + d.stops[stop_index_from    ].transfers_offset;
            transfer_t *tr_end = d.transfers + d.stops[stop_index_from + 1].transfers_offset;
            // int n_transfers_for_stop = tr_end - tr;
            for ( ; tr < tr_end ; ++tr) {
                int stop_index_to = tr->target_stop;
                int time_to = time_from + (int) (tr->dist_meters / speed_meters_sec + 60);
                router_state_t *state_to = states + stop_index_to;
                D printf("    target %d (%s) \n", 
                         stop_index_to, transit_data_stop_id_for_index(&d, stop_index_to));
                if (r.best_time[stop_index_to] > time_to) {
                    D printf ("      setting %d %d\n", stop_index_to, time_to);
                    state_to->arrival_time = time_to;
                    r.best_time[stop_index_to] = time_to;
                    state_to->back_route = WALK; // need sym const for walk distinct from NONE
                    state_to->back_stop = stop_index_from;
                    state_to->back_trip_id = "walk";
                    state_to->board_time = state_from->arrival_time;
                    flag_routes_for_stop (&r, stop_index_to);
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
        char *stop_id = transit_data_stop_id_for_index (&(r.tdata), s);
        printf("%6s ", stop_id);
        router_state_t *state = r.states + s;
        for (int round = 0; round < RRRR_MAX_ROUNDS; ++round, state += r.tdata.nstops) {
            printf("%8s ", timetext(state->arrival_time));
        }
        printf("\n");
    }
}

//assumes little endian http://stackoverflow.com/a/3974138/778449
void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;

    for (i=size-1;i>=0;i--)
    {
        for (j=7;j>=0;j--)
        {
            byte = b[i] & (1<<j);
            byte >>= j;
            printf("%u", byte);
        }
    }
    puts("");
}

bool router_route(router_t *prouter, router_request_t *preq) {
    router_t router = *prouter; // why copy?
    router_request_t req = *preq;
    int nstops = router.tdata.nstops;
    int trip_id_width = router.tdata.trip_id_width;
    int date_mask = 1 << 4; // as a demo, search on a specific day of the schedule

    // set initial state
    router_state_t *states = router.states;
    for (int s = 0; s < router.table_size; ++s)
        states[s].arrival_time = INF;
    for (int s = 0; s < nstops; ++s)
        router.best_time[s] = INF;
    router.best_time[req.from] = req.time;
    states[req.from].arrival_time = req.time;
    bitset_reset(router.updated_stops);
    bitset_set(router.updated_stops, req.from);
    // apply transfers to initial state, which also initializes updated routes bitset
    apply_transfers(router, 0, req.walk_speed);
    //T dump_results(prouter);
    for (int round = 0; round < RRRR_MAX_ROUNDS; ++round) {
        D printf("round %d\n", round);
        states = router.states + round * nstops;
        for (int route = bitset_next_set_bit (router.updated_routes, 0); route >= 0;
                 route = bitset_next_set_bit (router.updated_routes, route + 1)) {
            D printf("  route %d\n", route);
            T transit_data_dump_route(&(router.tdata), route);
            int *s_end, *t_end;    // pointers one element beyond the end of arrays
            /* TODO: restrict pointers ? */ 
            int *s = transit_data_stops_for_route(router.tdata, route, &s_end);
            int *t = transit_data_stoptimes_for_route(router.tdata, route, &t_end);
            int route_nstops = s_end - s;
            char *trip_ids = transit_data_trip_ids_for_route_index(&(router.tdata), route); 
            int *trip_masks = transit_data_trip_masks_for_route_index(&(router.tdata), route); 
            char *trip_id = NULL;
            int trip_index = NONE;       // trip index within the route
            int board_stop = NONE; // stop index where that trip was boarded
            int board_time = NONE; // time when that trip was boarded
            for ( ; s < s_end ; ++s, ++t) {
                int stop = *s;
                /* TODO: switch to CamelCase convention for OO classes */
                router_state_t *state = states + stop;
                T printf("    stop %d %s\n", stop, timetext(router.best_time[stop]));
                /* if we have not yet boarded a trip on this route, see if we can board one */
                if (trip_index == NONE) {
                    // should just set to the last trip in the list, and let it fall through
                    // if arr[stop] > *t; will also hit INFs.
                    if (router.best_time[stop] == INF) 
                        continue; 
                    // Scan forward to the latest trip that can be boarded, if any.
                    // (should scan backward to allow break at 1st)
                    T printf("hit non-inf stop %d\n", stop);
                    T transit_data_dump_route(&(router.tdata), route);
                    int trip_next = 0;
                    int *t_next = t;
                    while (t_next < t_end) {
                        T printf("    board option %d at %s\n", trip_next, timetext(*t_next));
                        if ((*t_next >= router.best_time[stop]) &&  // if this time is later than the best known for this stop
                            (date_mask & trip_masks[trip_next]))    // and if this trip is running
                                break;
                        trip_next += 1; // remembering that we start at -1 for NONE
                        t_next += route_nstops;
                    }
                    T printf("    final option %d at %s\n", trip_next, timetext(*t_next));
                    if (t_next < t_end) { // did not overshoot the end of the list
                        t = t_next;
                        trip_index = trip_next;
                        trip_id = trip_ids + trip_id_width * trip_index;
                        board_stop = stop;
                        board_time = *t_next;
                    }
                    continue;
                }
                /* TODO: typedef a time type? */
                if (*t < router.best_time[stop]) {
                    /* Only update if better than best known,
                       remembering that n transfers is increasing with each round,
                       and only Pareto-optimal paths count. */
                    state->arrival_time = *t;
                    router.best_time[stop] = *t;
                    state->back_route = route; 
                    state->back_trip_id = trip_id; 
                    state->back_stop = board_stop;
                    state->board_time = board_time;
                    bitset_set(router.updated_stops, stop); // mark stop for next round.
                    T printf("set stop %d to %s\n", stop, timetext(*t));
                } else if (router.best_time[stop] < *t) { // compare against PREVIOUS round only?
                    /* Check whether it is possible to board an earlier trip. */
                    while (trip_index > 0 && *(t - route_nstops) > router.best_time[stop]) {
                        t -= route_nstops; // skip to the same stop in the previous trip
                        --trip_index;
                        trip_id -= trip_id_width;
                        board_stop = stop;
                        board_time = *t;
                    }
                }
            } // end for (stop)
        } // end for (route)
        //if (round < RRRR_MAX_ROUNDS - 1) 
        apply_transfers(router, round, req.walk_speed);
        router.states[req.from].arrival_time = req.time;
        router.states[req.from].back_stop = -1;
        router.states[req.from].back_route = -1;
    } // end for (round)
    //dump_results(prouter); // DEBUG
    return true;
}

int router_result_dump(router_t *prouter, router_request_t *preq, char *buf, int buflen) {
    router_t router = *prouter;
    router_request_t req = *preq;
    int round = (RRRR_MAX_ROUNDS - 1);
    int count = 0;
    int s = req.to;
    char *b = buf;
    char *b_end = buf + buflen;
    //round = -1;
    while (round >= 0) {
        router_state_t *states = router.states + router.tdata.nstops * round;
        if (states[s].arrival_time == INF) {
            round -= 1;
            continue;
        } 
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

        b += sprintf (b, "%16s %8s %8s %8s %8s \n", trip_id, 
            last_stop_id, cboard, this_stop_id, calight);
        if (b > b_end) {
            printf ("buffer overflow\n");
            break;
        }
        if (last_stop == req.from) 
            break;
        if (count++ > 20) break;
        if (route >= 0)
            round -= 1;
        s = last_stop;
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
                timetext(req->time), req->time, req->walk_speed);
}

