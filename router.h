/* router.h */

#ifndef _ROUTER_H
#define _ROUTER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "tdata.h"
#include "bitset.h"
#include "util.h"

// Supplemented with the associated stop index, a router_state_t describes a leg of an itinerary.
typedef struct router_state router_state_t;
struct router_state {
    rtime_t time;        // The time when this stop was reached
    int back_stop;       // The index of the previous stop in the itinerary
    int back_route;      // The index of the route used to travel from back_stop to here, or WALK
    rtime_t board_time;  // The time at which the trip within back_route left back_stop
    char *back_trip_id;  // A text description of the trip used within back_route
};

// Scratch space for use by the routing algorithm.
// Making this opaque requires more dynamic allocation.
typedef struct router router_t;
struct router {
    tdata_t tdata;          // The transit / timetable data tables
    rtime_t *best_time;     // The best known time at each stop 
    router_state_t *states; // One router_state_t per stop, per round
    BitSet *updated_stops;  // Used to track which stops improved during each round
    BitSet *updated_routes; // Used to track which routes might have changed during each round

    // We should move more routing state in here, like round and sub-scratch pointers.
};

typedef struct router_request router_request_t;
struct router_request {
    int from;           // start stop index from the user's perspective, independent of arrive_by
    int to;             // destination stop index from the user's perspective, independent of arrive_by
    time_t time;        // the departure or arrival time at which to search
    double walk_speed;  // in meters per second
    bool arrive_by;     // whether the given time is an arrival time rather than a departure time
    rtime_t time_cutoff;// the latest (or earliest in arrive_by) time to reach the destination
    int max_transfers;  // the largest number of transfers to allow in the result
};

void router_setup(router_t*, tdata_t*);

bool router_request_from_qstring(router_request_t*);

void router_request_dump(router_t*, router_request_t*);

void router_request_initialize(router_request_t*);

void router_request_randomize(router_request_t*);

bool router_request_reverse(router_t*, router_request_t*);

void router_teardown(router_t*);

bool router_route(router_t*, router_request_t*);

int router_result_dump(router_t*, router_request_t*, char *buf, int buflen); // return num of chars written

#endif // _ROUTER_H

