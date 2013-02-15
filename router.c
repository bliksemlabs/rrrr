/* router.c : the main routing algorithm */

#include "router.h"
#include "util.h"
#include "config.h"
#include "qstring.h"
#include "transitdata.h"
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
    if (router->best_time  == NULL || router->states == NULL)
        die("failed to allocate router scratch space");
    
}

static inline void router_reset(router_t router) {

}

void router_teardown(router_t *router) {
    free(router->best_time);
    free(router->states);
}

static inline void apply_transfers(router_t r, int fround, int tround, float speed_meters_sec) {
    transit_data_t d = r.tdata;
    int nstops = d.nstops;
    router_state_t *fstates = r.states + (fround * nstops);
    router_state_t *tstates = r.states + (tround * nstops);
    for (int s0 = 0; s0 < nstops; ++s0) {
        router_state_t *state0 = fstates + s0;
        int t0 = state0->arrival_time;
        if (t0 != INF) { 
            // printf("transfer at %d\n", s0);
            // damn this is inefficient
            stop_t stop0 = d.stops[s0];
            stop_t stop1 = d.stops[s0+1];
            transfer_t *tr0 = d.transfers + stop0.transfers_offset;
            transfer_t *tr1 = d.transfers + stop1.transfers_offset;
            for ( ; tr0 < tr1 ; ++tr0) {
                int s1 = (*tr0).target_stop;
                int t1 = t0 + (int)((*tr0).dist_meters / speed_meters_sec);
                router_state_t *state1 = tstates + s1;
                // printf("   target %d\n", s1);
                if (state1->arrival_time > t1) {
                    // printf("    setting %d %d\n", s1, t1);
                    state1->arrival_time = t1;
                    state1->back_route = WALK; // need sym const for walk distinct from NONE
                    state1->back_stop = s0;
                    state1->back_trip_id = "walk";
                    state1->board_time = state0->arrival_time;
                }
            } 
        }
    }
}


static void dump_results(router_t *prouter) {
    router_t r = *prouter;
    printf("STOP ARRIVAL\n");
    for (int s = 0; s < r.tdata.nstops; ++s) {
        char *stop_id = transit_data_stop_id_for_index(&(r.tdata), s);
        printf("%6s ", stop_id);
        router_state_t *state = r.states + s;
        for (int round = 0; round < RRRR_MAX_ROUNDS; ++round, state += r.tdata.nstops) {
            printf("%8s ", timetext(state->arrival_time));
        }
        printf("\n");
    }
}


bool router_route(router_t *prouter, router_request_t *preq) {
    router_t router = *prouter;
    router_request_t req = *preq;
    int nstops = router.tdata.nstops;
    int nroutes = router.tdata.nroutes;
    // clear rounds 0 and 1 (no memset for ints).
    // it is not necessary to clear the other . they will only be read where arrivals are set.
    for (router_state_t *s = router.states, *s_end = router.states + nstops + nstops; s < s_end; ++s) { 
        (*s).arrival_time = INF; 
    }
    // use round 1 fields for "prev round" at round 0
    router_state_t *states_prev = router.states + nstops; 
    // set initial state
    states_prev[req.from].arrival_time = req.time; 
    // apply transfers to initial state
    apply_transfers(router, 1, 1, req.walk_speed);
    //dump_results(prouter); // DEBUG
    for (int round = 0; round < RRRR_MAX_ROUNDS; ++round) {
        //printf("round %d\n", round);
        router_state_t *states = router.states + round * nstops;
        if (round > 0) {
            states_prev = states - nstops;
            memcpy(states, states_prev, nstops * sizeof(router_state_t));
        }
        for (int route = 0; route < nroutes; ++route) {
            //if (route % 100 == 0) 
            //    printf("  route %d\n", route);
            // transit_data_dump_route(&(router.tdata), route);
            int *s_end, *t_end;    // pointers one element beyond the end of arrays
            int *s = transit_data_stops_for_route(router.tdata, route, &s_end);
            int *t = transit_data_stoptimes_for_route(router.tdata, route, &t_end);
            int route_nstops = s_end - s;
            char *trip_ids = transit_data_trip_ids_for_route_index(&(router.tdata), route); 
            char *trip_id = NULL;
            int trip = NONE;       // trip index within the route
            int trip_id_width = router.tdata.trip_id_width;
            int board_stop = NONE; // stop index where that trip was boarded
            int board_time = NONE; // time when that trip was boarded
            for ( ; s < s_end ; ++s, ++t) {
                int stop = *s;
                router_state_t *state = states + stop;
                router_state_t *state_prev = states_prev + stop;
                //printf("    stop %d %s\n", stop, timetext(arr_prev[stop]));
                /* if we have not yet boarded a trip on this route, see if we can board one */
                if (trip == NONE) {
                    // should just set to the last trip in the list, and let it fall through
                    // if arr[stop] > *t; will also hit INFs.
                    if (state_prev->arrival_time == INF) continue; // note comparison against PREVIOUS to enforce xfer invariant
                    // Scan forward to the latest trip that can be boarded, if any.
                    // (should scan backward to allow break at 1st)
                    //printf("hit non-inf stop %d\n", stop);
                    //transit_data_dump_route(&(router.tdata), route);
                    int trip_next = 0;
                    int *t_next = t;
                    while (t_next < t_end && *t_next < state_prev->arrival_time) {
                        //printf("    board option %d at %s\n", trip_next, timetext(*t_next));
                        trip_next += 1; // remembering that we start at -1 for NONE
                        // increment tripid as well?
                        t_next += route_nstops;
                    }
                    //printf("    final option %d at %s\n", trip_next, timetext(*t_next));
                    if (t_next < t_end) { // did not overshoot the end of the list
                        t = t_next;
                        trip = trip_next;
                        trip_id = trip_ids + trip_id_width * trip;
                        board_stop = stop;
                        board_time = *t_next;
                    }
                    continue;
                }
                // typedef a time_t?
                if (*t < state->arrival_time) { // we do have to check .LT., right?
                    state->arrival_time = *t;
                    state->back_route = route; // (should be route?)
                    state->back_trip_id = trip_id; 
                    state->back_stop = board_stop;
                    state->board_time = board_time;
                    // state.back_trip = ...
                    //printf("set stop %d to %s\n", stop, timetext(*t));
                } else if (state_prev->arrival_time < *t) { // compare against PREVIOUS round
                    // If last round's arrival time at this stop is before arrival time at this stop on the current trip,
                    // check whether it is possible to board an earlier trip.
                    while (trip > 0 && *(t - route_nstops) > state_prev->arrival_time) {
                        t -= route_nstops; // skip to the same stop in the previous trip
                        --trip;
                        trip_id -= trip_id_width;
                        board_stop = stop;
                        board_time = *t;
                    }
                }
            } // end for (stop)
        } // end for (route)
        //if (round < RRRR_MAX_ROUNDS - 1) 
        apply_transfers(router, round, round, req.walk_speed);
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

        char *route_id;
        if (route == -2)
            route_id = "walk";
        else if (route == -1)
            route_id = "init";

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
    req->from = rrrrandom(2268);
    req->to = rrrrandom(2268);
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

