#ifndef _ROUTER_REQUEST_H
#define _ROUTER_REQUEST_H

#include "rrrr_types.h"
#include "router.h"
#include "util.h"
#include "config.h"
#include "router_result.h"

void router_request_initialize(router_request_t *req);
void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime);
void router_request_randomize (router_request_t *req, tdata_t *tdata);
bool router_request_reverse(router_t *router, router_request_t *req);
bool router_request_reverse_all(router_t *router, router_request_t *req, router_request_t *ret, uint8_t *ret_n, uint8_t i_rev);

/* Use the itineraries in the plan_t to build reversals for time-wait compression.
 * Make reversal requests between the departure and the arrival of each itinerary.
 * Filter out itineraries that depart more than a travel duration after the best_arrival.
 */
bool router_request_reverse_plan(router_t *router, router_request_t *req, router_request_t *ret, uint8_t *ret_n, plan_t *plan);
time_t router_request_to_date (router_request_t *req, tdata_t *tdata, struct tm *tm_out);
time_t router_request_to_epoch (router_request_t *req, tdata_t *tdata, struct tm *tm_out);
bool range_check(router_request_t *req, tdata_t *router);
void router_request_dump(router_request_t *req, tdata_t *tdata);
#endif
