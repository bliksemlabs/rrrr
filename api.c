/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

/* api.c : snippets for using the routerengine */

#include "config.h"
#include "api.h"
#include "router_result.h"
#include "router_request.h"

/* Use first departure if someone wants to leave right now.
 * This means that longer wait times might occur later.
 * This is also the preference for ONBOARD searches.
 */
bool router_route_first_departure (router_t *router, router_request_t *req, plan_t *plan) {
    router_reset (router);

    if ( ! router_route (router, req) ) {
        return false;
    }

    if ( ! router_result_to_plan (plan, router, req) ) {
        return false;
    }

    return true;
}

/* Use naive reversal only if you want to show the very best arrival time
 * considering the presented location unconstrainted by other parameters
 * such as walking time or number of transfers.
 *
 * When searching clockwise we will board any vehicle_journey that will bring us at
 * the earliest time at any destination location. If we have to wait at
 * some stage for a connection, and this wait time exceeds the frequency
 * of the ingress network, we may suggest a later departure decreases
 * overal waitingtime.
 *
 * To compress waitingtime we employ a reversal. A clockwise search
 * departing at 9:00am and arriving at 10:00am is observed as was
 * requested: what vehicle_journey allows to arrive at 10:00am? The counter clockwise
 * search starts at 10:00am and offers the last possible arrival at 9:15am.
 * This bounds our searchspace between 9:15am and 10:00am.
 *
 * Because of the memory structure. We are not able to render an arrive-by
 * search, therefore the second arrival will start at 9:15am and should
 * render exactly the same vehicle_journey. This is not always true, especially not
 * when there are multiple paths with exactly the same transittime.
 *
 *
 * For an arrive_by counter clockwise search, we must make the result
 * clockwise. Only one reversal is required. For the more regular clockwise
 * search, the compression is handled in the first reversal (ccw) and made
 * clockwise in the second reversal.
 */
bool router_route_naive_reversal (router_t *router, router_request_t *req, plan_t *plan) {
    uint8_t i;
    uint8_t n_reversals = req->arrive_by ? 1 : 2;

    router_reset (router);

    if ( ! router_route (router, req) ) {
        return false;
    }

    for (i = 0; i < n_reversals; ++i) {
        if ( ! router_request_reverse (router, req)) {
            return false;
        }

        router_reset (router);

        if ( ! router_route (router, req)) {
            return false;
        }
    }

    if ( ! router_result_to_plan (plan, router, req) ) {
        return false;
    }

    return true;
}

/* Use the advanced reversal if you want to show the end user all
 * the alternatives possible given the choosen departure.
 */
bool router_route_full_reversal (router_t *router, router_request_t *req, plan_t *plan) {
    router_request_t req_storage[RRRR_DEFAULT_MAX_ROUNDS * RRRR_DEFAULT_MAX_ROUNDS];
    uint8_t i_rev;
    uint8_t n_req;
    uint8_t n2_req;

    router_reset (router);

    if ( ! router_route (router, req) ) {
        return false;
    }

    if ( ! req->arrive_by &&
           req->from_stop_point != NONE &&
         ! router_result_to_plan (plan, router, req) ) {
        return false;
    }

    /* We first add virtual request so we will never do them again */
    for (n_req = 0; n_req < plan->n_itineraries; ++n_req) {
        req_storage[n_req] = *req;
        req_storage[n_req].time = plan->itineraries[n_req].legs[0].t0;
        req_storage[n_req].max_transfers = plan->itineraries[n_req].n_rides - 1;
    }

    /* Fetch the first possible time to get out of here by transit */
    req_storage[n_req] = *req;
    if ( ! req_storage[n_req].arrive_by) {
        rtime_t earliest_departure = UNREACHED;
        spidx_t i_stop;

        for (i_stop = 0; i_stop < router->tdata->n_stop_points; ++i_stop) {
            if (router->states_board_time[i_stop] != UNREACHED) {
                earliest_departure = MIN(earliest_departure, router->states_board_time[i_stop]);
            }
        }
        req_storage[n_req].time = earliest_departure;
    }
    i_rev = n_req;
    n_req++;

    /* first reversal, always required */
    if ( ! router_request_reverse_all (router, &req_storage[i_rev], req_storage, &n_req)) {
        return false;
    }

    i_rev++;
    n2_req = n_req;

    for (; i_rev < n_req; ++i_rev) {
        router_reset (router);

        if ( ! router_route (router, &req_storage[i_rev]) ) {
            return false;
        }

        if ( ! req_storage[i_rev].arrive_by &&
             ! router_result_to_plan (plan, router, &req_storage[i_rev])) {
            return false;
        }

        if ( ! req->arrive_by && i_rev < n2_req &&
             ! router_request_reverse_all (router, &req_storage[i_rev], req_storage, &n_req)) {
            return false;
        }
    }

    return true;
}
