/* router.h */

#ifndef _ROUTER_H
#define _ROUTER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "tdata.h"
#include "bitset.h"
#include "util.h"

typedef struct router_state router_state_t;
struct router_state {
    rtime_t time;
    int back_stop;
    int back_route;
    rtime_t board_time;
    char *back_trip_id;
};

typedef struct router router_t;
struct router {
    tdata_t tdata;
    // scratch space for solving
    // making this opaque requires more dynamic allocation
    int table_size;
    rtime_t *best_time;
    router_state_t *states;
    BitSet *updated_stops;
    BitSet *updated_routes;
    // maybe we should store more routing state in here, like round and sub-scratch pointers
};

typedef struct router_request router_request_t;
struct router_request {
    int from;
    int to;
    time_t time;
    double walk_speed;
    bool arrive_by; 
};

void router_setup(router_t*, tdata_t*);

bool router_request_from_qstring(router_request_t*);

void router_request_dump(router_t*, router_request_t*);

void router_request_randomize(router_request_t*);

void router_teardown(router_t*);

bool router_route(router_t*, router_request_t*, bool arrv);

int router_result_dump(router_t*, router_request_t*, char *buf, int buflen); // return num of chars written

#endif // _ROUTER_H

