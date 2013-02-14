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
    router->best = malloc(sizeof(int) * td->nstops); 
    router->arrivals  = malloc(sizeof(int) * router->table_size);
    router->back_route = malloc(sizeof(int) * router->table_size);
    router->back_stop = malloc(sizeof(int) * router->table_size);
    if (router->arrivals  == NULL ||
        router->back_route == NULL ||
        router->back_stop == NULL)
        die("failed to allocate router scratch space");
    
}

static inline void router_reset(router_t router) {

}

void router_teardown(router_t *router) {
    free(router->arrivals);
    free(router->back_route);
    free(router->back_stop);
}

static inline void apply_transfers(router_t r, int round, float speed_meters_sec) {
    transit_data_t d = r.tdata;
    int nstops = d.nstops;
    int *arr = r.arrivals + (round * nstops);
    int *back_route = r.back_route; // + (round * nstops);
    int *back_stop = r.back_stop; // + (round * nstops);
    for (int s0 = 0; s0 < nstops; ++s0) {
        int t0 = arr[s0]; 
        if (t0 != INF) { 
            // damn this is inefficient
            stop_t stop0 = d.stops[s0];
            stop_t stop1 = d.stops[s0+1];
            transfer_t *tr0 = d.transfers + stop0.transfers_offset;
            transfer_t *tr1 = d.transfers + stop1.transfers_offset;
            for ( ; tr0 < tr1 ; ++tr0) {
                int s1 = (*tr0).target_stop;
                int t1 = t0 + (int)((*tr0).dist_meters / speed_meters_sec);
                if (arr[s1] > t1) {
                    arr[s1] = t1;
                    back_route[s1] = WALK; // need sym const for walk distinct from NONE
                    back_stop[s1] = s0;
                }
            } 
        }
    }
}

static void dump_results(router_t *prouter) {
    router_t r = *prouter;
    printf("STOP ARRIVAL\n");
    for (int s = 0; s < r.tdata.nstops; ++s) {
        printf("%4d ", s);
        int *a = r.arrivals + s;
        for (int round = 0; round < RRRR_MAX_ROUNDS; ++round, a += r.tdata.nstops) {
            printf("%8s ", timetext(*a));
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
    // it is not necessary to clear the other tables. they will only be read where arrivals are set.
    for (int *p = router.arrivals, *pend = router.arrivals + nstops + nstops; p < pend; ++p) { 
        *p = INF; 
    }
    // use round 1 fields for "prev round" at round 0
    int *arr_prev = router.arrivals + nstops; 
    // set initial state (maybe group as a "state" struct?
    arr_prev[req.from] = req.time; 
    for (int round = 0; round < RRRR_MAX_ROUNDS; ++round) {
        //printf("round %d\n", round);
        int *arr = router.arrivals + round * nstops;
        int *back_route = router.back_route; //+ round * nstops;
        int *back_stop = router.back_stop; //+ round * nstops;
        if (round > 0) {
            arr_prev = arr - nstops;
            memcpy(arr, arr_prev, nstops * sizeof(int));
        }
        for (int route = 0; route < nroutes; ++route) {
            //if (route % 100 == 0) 
            //    printf("  route %d\n", route);
            // transit_data_dump_route(&(router.tdata), route);
            int *s_end, *t_end;    // pointers one element beyond the end of arrays
            int *s = transit_data_stops_for_route(router.tdata, route, &s_end);
            int *t = transit_data_stoptimes_for_route(router.tdata, route, &t_end);
            int route_nstops = s_end - s;
            int trip = NONE;       // trip index within the route
            int board_stop = NONE; // stop index where that trip was boarded
            for ( ; s < s_end ; ++s, ++t) {
                int stop = *s;
                //printf("    stop %d %s\n", stop, timetext(arr_prev[stop]));
                if (trip == NONE) {
                    // should just set to the last trip in the list, and let it fall through
                    // if arr[stop] > *t; will also hit INFs.
                    if (arr_prev[stop] == INF) continue; // note comparison against PREVIOUS to enforce xfer invariant
                    // Scan forward to the latest trip that can be boarded, if any.
                    // (should scan backward to allow break at 1st)
                    //printf("hit non-inf stop %d\n", stop);
                    //transit_data_dump_route(&(router.tdata), route);
                    int trip_next = 0;
                    int *t_next = t;
                    while (t_next < t_end && *t_next < arr_prev[stop]) {
                        //printf("    board option %d at %s\n", trip_next, timetext(*t_next));
                        trip_next += 1; // remembering that we start at -1 for NONE
                        t_next += route_nstops;
                    }
                    //printf("    final option %d at %s\n", trip_next, timetext(*t_next));
                    if (t_next < t_end) { // did not overshoot the end of the list
                        t = t_next;
                        trip = trip_next;
                        board_stop = stop;
                    }
                    continue;
                }
                // typedef a time_t?
                if (*t < arr[stop]) { // we do have to check .LT., right?
                    arr[stop] = *t;
                    back_route[stop] = route; // (should be route?)
                    back_stop[stop] = board_stop;
                    //printf("set stop %d to %s\n", stop, timetext(*t));
                } else if (arr_prev[stop] < *t) { // compare against PREVIOUS round
                    // If last round's arrival time at this stop is before arrival time at this stop on the current trip,
                    // check whether it is possible to board an earlier trip.
                    while (trip > 0 && *(t - route_nstops) > arr_prev[stop]) {
                        t -= route_nstops; // skip to the same stop in the previous trip
                        --trip;
                        board_stop = stop;
                    }
                }
            } // end for (stop)
        } // end for (route)
        apply_transfers(router, round, req.walk_speed);
        back_route[req.from] = NONE;
        back_stop[req.from] = NONE;
    } // end for (round)
    //dump_results(prouter);
    return true;
}

void router_result_dump(router_t *prouter, router_request_t *preq) {
    router_t router = *prouter;
    router_request_t req = *preq;
    printf("---- routing result ----\n");
    int last_round = router.tdata.nstops * (RRRR_MAX_ROUNDS - 1);

    int *arr = router.arrivals + last_round;
    int *back_route = router.back_route;// + last_round;
    int *back_stop = router.back_stop;// + last_round;

    int count = 0;
    for (int s = req.to; ; s = back_stop[s]) {
        char *stop_id = transit_data_stop_id_for_index(&(router.tdata), s);
        printf ("%8s stop %6s [%06d] via route [%2d]\n", timetext(arr[s]), stop_id, s, back_route[s]);
        if (back_stop[s] == NONE) {
            printf("---- end of routing result ----\n");
            break;
        }
        if (count++ > 10) break;
    }        
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
    req->from = rrrrandom(5500);
    req->to = rrrrandom(5500);
    req->time = 3600 * 6 + rrrrandom(3600 * 17);
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

