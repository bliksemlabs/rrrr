/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "router_result.h"
#include "router_request.h"
#include "street_network.h"

bool router_route_first_departure (router_t *router, router_request_t *req, plan_t *plan);
bool router_route_all_departures (router_t *router, router_request_t *req, plan_t *plan);
bool router_route_naive_reversal (router_t *router, router_request_t *req, plan_t *plan);
bool router_route_full_reversal (router_t *router, router_request_t *req, plan_t *plan);
uint32_t tdata_n_departures_since (tdata_t *td, spidx_t *sps, spidx_t n_stops, rtime_t *result, uint32_t n_results, rtime_t since, rtime_t until);
