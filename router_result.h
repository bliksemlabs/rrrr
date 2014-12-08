#ifndef _ROUTER_RESULT_H
#define _ROUTER_RESULT_H

#include "config.h"
#include "util.h"
#include "rrrr_types.h"
#include "router.h"

/* A leg represents one ride or walking transfer. */
typedef struct leg leg_t;
struct leg {
    /* journey_pattern index */
    uint32_t journey_pattern;

    /* trip index */
    uint32_t trip;

    /* from stop index */
    uint32_t s0;

    /* to stop index */
    uint32_t s1;

    /* start time */
    rtime_t  t0;

    /* end time */
    rtime_t  t1;

    #ifdef RRRR_FEATURE_REALTIME
    /* start delay */
    int16_t d0;

    /* end delay */
    int16_t d1;
    #endif
};

/* An itinerary is a chain of legs leading from one place to another. */
typedef struct itinerary itinerary_t;
struct itinerary {
    uint32_t n_rides;
    uint32_t n_legs;
    leg_t legs[RRRR_DEFAULT_MAX_ROUNDS * 2 + 1];
};


/* A plan is several pareto-optimal itineraries connecting the same two stops. */
typedef struct plan plan_t;
struct plan {
    router_request_t req;
    uint32_t n_itineraries;
    itinerary_t itineraries[RRRR_DEFAULT_MAX_ROUNDS];
};


bool router_result_to_plan (struct plan *plan, router_t *router, router_request_t *req);

/* return num of chars written */
uint32_t router_result_dump(router_t*, router_request_t*, char *buf, uint32_t buflen);

#endif
