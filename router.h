/* router.h */

#ifndef _ROUTER_H
#define _ROUTER_H

#include "config.h"
#include "router.h"

#include "rrrr_types.h"
#include "tdata.h"
#include "bitset.h"
#include "hashgrid.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* When associated with a stop index,
 * a router_state_t describes a leg of an itinerary.
 */
typedef struct router_state router_state_t;
/* We could potentially remove the back_time from router_state,
 * but this requires implementing some lookup functions and storing
 * the back_trip_stop rather than the back_stop (global stop index):
 * a trip can pass through a stop more than once.
 */
/* TODO rename members to ride_from, walk_from, journey_pattern, trip, */
struct router_state {
   /* The index of the journey_pattern used to travel from back_stop to here, or WALK  */
    uint32_t back_journey_pattern;
    /* The index of the trip used to travel from back_stop */
    uint32_t back_trip;

    /* The index of the previous stop in the itinerary */
    spidx_t ride_from;

    /* Second phase footpath/transfer results */
    /* The stop from which this stop was reached by walking (2nd phase) */
    spidx_t walk_from;

    /* Second phase footpath/transfer results */
    /* The time when this stop was reached by walking (2nd phase) */
    rtime_t  walk_time;

    /* The time when this stop was reached */
    rtime_t  time;
    /* The time at which the trip within back_journey_pattern left back_stop */
    rtime_t  board_time;

    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    uint16_t back_journey_pattern_point;
    uint16_t journey_pattern_point;
    #endif
};

/* Scratch space for use by the routing algorithm.
 * Making this opaque requires more dynamic allocation.
 */
typedef struct router router_t;
struct router {
    /* The transit / timetable data tables */
    tdata_t *tdata;

    /* The best known time at each stop */
    rtime_t *best_time;

    /* One router_state_t per stop, per round */
    router_state_t *states;

    /* Used to track which stops improved during each round */
    BitSet *updated_stops;

    /* Used to track which journey_patterns might have changed during each round */
    BitSet *updated_journey_patterns;

    /* Used to track to which stops we changed the walk_time during each round */
    BitSet *updated_walk_stops;

#ifdef RRRR_BANNED_JOURNEY_PATTERNS_BITMASK
    /* Used to ban journey_patterns and in the final clockwise search optimise */
    BitSet *banned_journey_patterns;
#endif

    spidx_t origin;
    spidx_t target;

    calendar_t day_mask;
    serviceday_t servicedays[3];

#ifdef RRRR_FEATURE_LATLON
    HashGrid hg;
#endif
    /* TODO: We should move more routing state in here,
     * like round and sub-scratch pointers.
     */
};

struct journey_pattern_cache {
    journey_pattern_t *this_jp;
    uint32_t *journey_pattern_points; /* TODO: spidx_t */
    uint8_t  *journey_pattern_point_attributes;
    trip_t   *trips_in_journey_pattern;
    calendar_t *trip_masks;
    uint32_t jp_index;
    bool jp_overlap;
};

typedef struct journey_pattern_cache journey_pattern_cache_t;


/* FUNCTION PROTOTYPES */

bool router_setup(router_t*, tdata_t*);

void router_reset(router_t *router);

void router_teardown(router_t*);

bool router_route(router_t*, router_request_t*);

void router_round(router_t *router, router_request_t *req, uint8_t round);

#endif /* _ROUTER_H */

