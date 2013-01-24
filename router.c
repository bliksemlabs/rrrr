/* router.c : the main routing algorithm */
#include "router.h"
#include "util.h"
#include "config.h"
#include "qstring.h"
#include "transitdata.h"
#include <stdlib.h>
#include <fcgi_stdio.h>
#include <string.h>

void router_setup(router_t *router, transit_data_t *td) {
    router->tdata = *td;
    router->table_size = td->nstops * CONFIG_MAX_ROUNDS;
    router->best = malloc(sizeof(int) * td->nstops); 
    router->arrivals  = malloc(sizeof(int) * router->table_size);
    router->back_trip = malloc(sizeof(int) * router->table_size);
    router->back_stop = malloc(sizeof(int) * router->table_size);
    if (router->arrivals  == NULL ||
        router->back_trip == NULL ||
        router->back_stop == NULL)
        die("failed to allocate router scratch space");
    
}

inline static void router_reset(router_t router) {
}

void router_teardown(router_t *router) {
    free(router->arrivals);
    free(router->back_trip);
    free(router->back_stop);
}

static void apply_transfers(int round, int stop) {
    
}

static void dump_results(router_t *prouter) {
    router_t r = *prouter;
    printf("STOP ARRIVAL\n");
    for (int s = 0; s < r.tdata.nstops; ++s) {
        printf("%4d ", s);
        int *a = r.arrivals + s;
        char buf[16];
        for (int round = 0; round < CONFIG_MAX_ROUNDS; ++round, a += r.tdata.nstops) {
            timetext(buf, *a);
            printf("%8s ", buf);
        }
        printf("\n");
    }
}

bool router_route(router_t *prouter, router_request_t *preq) {
    router_t router = *prouter;
    router_request_t req = *preq;
    int nstops = router.tdata.nstops;
    int nroutes = router.tdata.nroutes;
    // clear arrivals. not necessary to clear other tables, they will only be read where arrivals are set.    
    for (int *p = router.arrivals, *pend = router.arrivals + nstops; p < pend; ++p)
        *p = INF; // no memset for ints
        
    // set initial state (maybe group all this together as "state"?
    router.arrivals[req.from] = req.time; 
    router.back_trip[req.from] = NONE; 
    router.back_stop[req.from] = NONE; 
    //apply_transfers(0, req.from);
    int setcount = 0;
    for (int round = 0; round < CONFIG_MAX_ROUNDS; ++round) {
        printf("round %d\n", round);
        int *arr = router.arrivals + round * nstops;
        int *back_trip = router.back_trip + round * nstops;
        int *back_stop = router.back_stop + round * nstops;
        if (round > 0)
            memcpy(arr, arr-nstops, nstops * sizeof(int));
        for (int route = 0; route < nroutes; ++route) {
            //if (route % 100 == 0) 
                //printf("  route %d\n", route);
            // transit_data_dump_route(&(router.tdata), route);
            int *s_end, *t_end;    // pointers one element beyond the end of arrays
            int *s = transit_data_stops_for_route(router.tdata, route, &s_end);
            int *t = transit_data_stoptimes_for_route(router.tdata, route, &t_end);
            int route_nstops = s_end - s;
            int trip = NONE;       // trip index within the route
            int board_stop = NONE; // stop index where that trip was boarded
            t -= route_nstops; // hackish handling of first boarding -- pointing outside the array
            for ( ; s < s_end; ++s, ++t) {
                int stop = *s;
                //printf("    stop %d\n", stop);
                if (trip == NONE) {
                    // should just set to the last trip in the list, and let it fall through
                    // if arr[stop] > *t; will also hit INFs.
                    //printf("stop %d arrival %d\n", stop, arr[stop]);
                    if (arr[stop] == INF) continue; // prefill results with NONE/INF or copy from R_r-1
                    // Scan forward to the latest trip that can be boarded, if any.
                    // (should scan backward to allow break at 1st)
                    //printf("hit non-inf stop %d\n", stop);
                    for (int *t_next = t + route_nstops; t_next < t_end && arr[stop] < *t_next; t_next += route_nstops) {
                        t = t_next;
                        trip += 1; // remembering that we start at -1 for NONE
                        board_stop = stop;
                    }
                    continue;
                }
                // typedef a time_t?
                if (*t < arr[stop]) {
                    arr[stop] = *t;
                    back_trip[stop] = trip; // (should be route?)
                    back_stop[stop] = board_stop;
                    //printf("set stop %d to %d\n", stop, *t);
                    ++setcount;
                } else if (arr[stop] < *t) { 
                    // If arrival time at this stop is before arrival time at this stop on the current trip,
                    // check whether it is possible to board an earlier trip.
                    while (trip > 0 && *(t - route_nstops) > arr[stop]) {
                        t -= route_nstops; // skip to the same stop in the previous trip
                        --trip;
                    }
                }
            } 
        }
    }
    printf ("set %d stops\n", setcount);
    //dump_results(prouter);
    return true;
}

inline static void set_defaults(router_request_t *req) {
    req->walk_speed = 1.3; // m/sec
    req->from = req->to = req->time = NONE; 
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
        } else {
            printf("unrecognized parameter: key=%s val=%s\n", key, val);
        }
    }
    return true;
}

void router_request_dump(router_request_t *req) {
    printf("from: %d\n"
           "to: %d\n"
           "time: %d\n"
           "speed: %f\n", req->from, req->to, req->time, req->walk_speed);
}

