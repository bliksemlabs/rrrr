#ifndef _ROUTER_RESULT_H
#define _ROUTER_RESULT_H

#include "config.h"
#include "util.h"
#include "rrrr_types.h"
#include "router.h"

/* A leg represents one ride or walking transfer. */
typedef struct leg leg_t;
struct leg {
    /* vj index */
    uint32_t vj;

    /* journey_pattern index */
    jpidx_t journey_pattern;

    /* from stop_point index */
    spidx_t sp_from;

    /* to stop_point index */
    spidx_t sp_to;

    /* start time */
    rtime_t  t0;

    /* end time */
    rtime_t  t1;

    #ifdef RRRR_FEATURE_REALTIME
    /* start journey_pattern_point index */
    uint16_t jpp0;

    /* end journey_pattern_point index */
    uint16_t jpp1;

    /* start delay */
    int16_t d0;

    /* end delay */
    int16_t d1;
    #endif
};


/* An itinerary is a chain of legs leading from one place to another. */
typedef struct itinerary itinerary_t;
struct itinerary {
    leg_t legs[RRRR_DEFAULT_MAX_ROUNDS * 2 + 1];
    uint8_t n_rides;
    uint8_t n_legs;
};


/* A plan is several pareto-optimal itineraries connecting the same two stops. */
typedef struct plan plan_t;
struct plan {
    itinerary_t itineraries[RRRR_DEFAULT_MAX_ROUNDS * RRRR_DEFAULT_MAX_ROUNDS];
    router_request_t req;
    uint8_t n_itineraries;
};

/* Structure to temporary store abstracted plans */
typedef struct result result_t;
struct result {
    /* from stop_point index */
    spidx_t sp_from;

    /* to stop_point index */
    spidx_t sp_to;

    /* start time */
    rtime_t  t0;

    /* end time */
    rtime_t  t1;

    /* modes in trip */
    uint8_t mode;

    /* transfers in trip */
    uint8_t n_transfers;
};

bool router_result_to_plan (plan_t *plan, router_t *router, router_request_t *req);

void router_result_sort (plan_t *plan);

/* return num of chars written */
uint32_t router_result_dump(router_t *router, router_request_t *req,
                            uint32_t(*render)(plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen),
                            char *buf, uint32_t buflen);

#endif
