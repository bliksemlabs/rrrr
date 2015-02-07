/* router.h */

#ifndef _ROUTER_H
#define _ROUTER_H

#include "config.h"
#include "router.h"

#include "rrrr_types.h"
#include "tdata.h"
#include "bitset.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* When associated with a stop_point index,
 * a router_state_t describes a leg of an itinerary.
 */

/* We could potentially remove the back_time from router_state,
 * but this requires implementing some lookup functions and storing
 * the back_vj_stop_point rather than the back_stop_point (global stop_point index):
 * a vehicle_journey can pass through a stop_point more than once.
 */

/* Scratch space for use by the routing algorithm.
 * Making this opaque requires more dynamic allocation.
 */
typedef struct router router_t;
struct router {
    /* The transit / timetable data tables */
    tdata_t *tdata;

    /* The best known time at each stop_point */
    rtime_t *best_time;

    /* The index of the journey_pattern used to travel from back_stop_point to here, or WALK  */
    jpidx_t *states_back_journey_pattern;

    /* The index of the vehicle_journey used to travel from back_stop_point */
    jp_vjoffset_t *states_back_vehicle_journey;

    /* The index of the previous stop_point in the itinerary */
    spidx_t *states_ride_from;

    /* Second phase footpath/transfer results */
    /* The stop_point from which this stop_point was reached by walking (2nd phase) */
    spidx_t *states_walk_from;

    /* Second phase footpath/transfer results */
    /* The time when this stop_point was reached by walking (2nd phase) */
    rtime_t *states_walk_time;

    /* The time when this stop_point was reached */
    rtime_t *states_time;
    /* The time at which the vehicle_journey within back_journey_pattern left back_stop_point */
    rtime_t *states_board_time;

    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    jppidx_t *states_back_journey_pattern_point;
    jppidx_t *states_journey_pattern_point;
    #endif

    /* Used to track which stop_points improved during each round */
    bitset_t *updated_stop_points;

    /* Used to track which journey_patterns might have changed during each round */
    bitset_t *updated_journey_patterns;

    /* Used to track to which stop_points we changed the walk_time during each round */
    bitset_t *updated_walk_stop_points;

#ifdef RRRR_BANNED_JOURNEY_PATTERNS_BITMASK
    /* Used to ban journey_patterns and in the final clockwise search optimise */
    bitset_t *banned_journey_patterns;
#endif

    spidx_t origin;
    spidx_t target;

    calendar_t day_mask;
    serviceday_t servicedays[3];
    uint8_t n_servicedays;

    /* TODO: We should move more routing state in here,
     * like round and sub-scratch pointers.
     */
};

/* FUNCTION PROTOTYPES */

bool router_setup(router_t*, tdata_t*);

void router_reset(router_t *router);

void router_teardown(router_t*);

bool router_route(router_t*, router_request_t*);

#endif /* _ROUTER_H */
