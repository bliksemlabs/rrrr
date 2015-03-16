/* Copyright 2013–2015 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "router_result.h"
#include "router_request.h"
#include "street_network.h"

bool router_route_first_departure (router_t *router, router_request_t *req, plan_t *plan);
bool router_route_naive_reversal (router_t *router, router_request_t *req, plan_t *plan);
bool router_route_full_reversal (router_t *router, router_request_t *req, plan_t *plan);
