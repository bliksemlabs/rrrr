/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "router_request.h"
#include "tdata.h"
#include "plan.h"

bool router_route_one_trip (tdata_t *tdata, router_request_t *req, plan_t *plan);
