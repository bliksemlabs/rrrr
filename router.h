/* router.h */

#ifndef _ROUTER_H
#define _ROUTER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "tdata.h"
#include "bitset.h"
#include "util.h"
#include "config.h"

/* When associated with a stop index, a router_state_t describes a leg of an itinerary. */
typedef struct router_state router_state_t;
/* We could potentially remove the back_time from router_state, but this requires implementing some 
   lookup functions and storing the back_trip_stop rather than the back_stop (global stop index): 
   a trip can pass through a stop more than once.
   TODO rename members to ride_from, walk_from, route, trip, ride_time, walk_time ? */
struct router_state {
    uint32_t back_stop;  // The index of the previous stop in the itinerary
    uint32_t back_route; // The index of the route used to travel from back_stop to here, or WALK
    uint32_t back_trip;  // The index of the trip used to travel from back_stop to here, or WALK
    rtime_t  time;       // The time when this stop was reached
    rtime_t  board_time; // The time at which the trip within back_route left back_stop
    /* Second phase footpath/transfer results */
    uint32_t walk_from;  // The stop from which this stop was reached by walking (2nd phase)
    rtime_t  walk_time;  // The time when this stop was reached by walking (2nd phase)
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


typedef enum trip_attributes {
    ta_none = 0,
    ta_accessible = 1,
    ta_toilet = 2,
    ta_wifi = 4
} trip_attributes_t;


typedef enum optimise {
    o_shortest  =   1, // output shortest time
    o_transfers =   2, // output least transfers
    o_all       = 255  // output all rides
} optimise_t;


typedef enum tmode {
    m_tram      =   1,
    m_subway    =   2,
    m_rail      =   4,
    m_bus       =   8,
    m_ferry     =  16,
    m_cablecar  =  32,
    m_gondola   =  64,
    m_funicular = 128,
    m_all       = 255
} tmode_t;


typedef struct router_request router_request_t;
struct router_request {
    uint32_t from;       // start stop index from the user's perspective, independent of arrive_by
    uint32_t to;         // destination stop index from the user's perspective, independent of arrive_by
    uint32_t via;        // preferred transfer stop index from the user's perspective, default: NONE
    uint32_t start_trip_route; // for onboard departure: route index on which to begin
    uint32_t start_trip_trip;  // for onboard departure: trip index within that route
    rtime_t time;        // the departure or arrival time at which to search (in internal rtime)
    rtime_t time_cutoff; // the latest (or earliest in arrive_by) acceptable time to reach the destination 
    double walk_speed;   // speed at which the user walks, in meters per second
    uint8_t walk_slack;  // an extra delay per transfer, in seconds 
    bool arrive_by;      // whether the given time is an arrival time rather than a departure time
    uint32_t max_transfers;  // the largest number of transfers to allow in the result
    uint32_t day_mask;   // bit for the day on which we are searching, relative to the timetable calendar
    uint8_t mode;        // selects the mode by a bitfield
    uint8_t trip_attributes; // select required attributes bitfield (from trips)
    uint8_t optimise;    // restrict the output to specific optimisation flags
    uint32_t n_banned_routes; // 1
    uint32_t n_banned_stops; // 1
    uint32_t n_banned_stops_hard; // 1
    uint32_t n_banned_trips; // 1
    uint32_t banned_route; // One route which is banned 
    uint32_t banned_stop; // One stop which is banned 
    uint32_t banned_trip_route; // One trip which is banned, this is its route
    uint32_t banned_trip_offset; // One trip which is banned, this is its tripoffset
    uint32_t banned_stop_hard; // One stop which is banned 
    bool intermediatestops; // Show intermetiastops in the output
};


/* ROUTING RESULT STRUCTURES */ // TODO add summary data like duration, begin/end times?

/* A leg represents one ride or walking transfer. */
struct leg {
    uint32_t s0;    // from stop index
    uint32_t s1;    // to stop index
    rtime_t  t0;    // start time
    rtime_t  t1;    // end time
    uint32_t route;
    uint32_t trip;
//    int32_t  delay; // signed realtime delay, in seconds
};

/* An itinerary is a chain of legs leading from one place to another. */
struct itinerary {
    uint32_t n_rides;
    uint32_t n_legs;
    struct leg legs[RRRR_MAX_ROUNDS * 2 + 1];
};

/* A plan is several pareto-optimal itineraries connecting the same two stops. */
struct plan {
    router_request_t req;
    uint32_t n_itineraries;
    struct itinerary itineraries[RRRR_MAX_ROUNDS];
};


/* FUNCTION PROTOTYPES */

void router_setup(router_t*, tdata_t*);

bool router_request_from_qstring(router_request_t*, tdata_t *tdata);

void router_request_dump(router_t*, router_request_t*);

void router_request_initialize(router_request_t*);

void router_request_randomize(router_request_t*, tdata_t *tdata);

bool router_request_reverse(router_t*, router_request_t*);

void router_teardown(router_t*);

bool router_route(router_t*, router_request_t*);

void router_result_to_plan (struct plan *plan, router_t *router, router_request_t *req);

uint32_t router_result_dump(router_t*, router_request_t*, char *buf, uint32_t buflen); // return num of chars written

void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime);

time_t req_to_date (router_request_t *req, tdata_t *tdata, struct tm *tm_out);

time_t req_to_epoch (router_request_t *req, tdata_t *tdata, struct tm *tm_out);

uint32_t transfer_distance (tdata_t *d, uint32_t stop_index_from, uint32_t stop_index_to);

#endif // _ROUTER_H

