/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

/* api.c : snippets for using the routerengine */

#include "config.h"
#include "api.h"
#include "bitset.h"
#include "plan.h"
#include "util.h"
#include "set.h"
#include "street_network.h"
#include "plan_render_text.h"
#include "router_result.h"

#include <stdlib.h>

/* Use first departure if someone wants to leave right now.
 * This means that longer wait times might occur later.
 * This is also the preference for ONBOARD searches.
 */
bool router_route_first_departure (router_t *router, router_request_t *req, plan_t *plan) {
    router_reset (router);
    router_request_search_street_network (req, router->tdata);

    if ( ! router_route (router, req) ) {
        return false;
    }

    if ( ! router_result_to_plan (plan, router, req) ) {
        return false;
    }

    return true;
}

static void iterate_origins (router_t *router, router_request_t *req,
                             plan_t *plan, rtime_t *result, uint32_t n) {
    plan_t work_plan;
    uint32_t n_results = n;
    plan_init (&work_plan);

    while (n_results) {
        --n_results;
        req->time = result[n_results] + RTIME_ONE_DAY;

        req->max_transfers = RRRR_DEFAULT_MAX_ROUNDS - 1;
        /* time_cutoff may remain the highest value observed, if the search runs from last to first, but it seems we are missing some results if we don't apply it */
        router_reset (router);

        if ( router_route (router, req) ) {
            int16_t i_itin = 0;
            router_result_to_plan (&work_plan, router, req);
            for (;i_itin < work_plan.n_itineraries && plan->n_itineraries < (RRRR_DEFAULT_MAX_ROUNDS * RRRR_DEFAULT_MAX_ROUNDS); ++i_itin) {
                uint32_t i_plan = plan->n_itineraries;
                itinerary_t *b = &work_plan.itineraries[i_itin];
                bool duplicate = false;
                while (i_plan) {
                    itinerary_t *a;
                    i_plan--;
                    a = &plan->itineraries[i_plan];
                    if (b->n_legs == 0) {
                        /* feels like a hack, how can n_itineraries > 0, but n_legs = 0? */
                        duplicate = true;
                        break;
                    }
                    if (a->n_rides == b->n_rides && a->n_legs == b->n_legs && a->legs[1].sp_from == b->legs[1].sp_from && a->legs[a->n_legs - 1].sp_to == b->legs[b->n_legs - 1].sp_to &&
                        a->legs[1].t0 == b->legs[1].t0 && a->legs[a->n_legs - 1].t1 == b->legs[b->n_legs - 1].t1) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    plan->itineraries[plan->n_itineraries] = work_plan.itineraries[i_itin];
                    plan->n_itineraries++;
                }
            }
        }
    }
}


bool router_route_all_departures (router_t *router, router_request_t *req, plan_t *plan) {
    rtime_t *result = (rtime_t *) malloc(sizeof(rtime_t) * 64);
    uint32_t n_results;

    router_request_search_street_network (req, router->tdata);
    street_network_null_duration (&req->entry);
    /* router_reset (router); */

    n_results = tdata_n_departures_since (router->tdata,
                                          req->entry.stop_points, req->entry.n_points,
                                          result, 64,
                                          req->time - RTIME_ONE_DAY,
                                          req->time - RTIME_ONE_DAY + 1200);

    iterate_origins (router, req, plan, result, n_results);

    #if RRRR_MAX_FILTERED_OPERATORS > 0 || RRRR_MAX_BANNED_OPERATORS > 0
    if (false) {
        bitset_t *bitset_op;
        bitset_op = bitset_new (router->tdata->n_operator_ids);
        bitset_clear (bitset_op);

        if (plan_get_operators (plan, router->tdata, bitset_op)) {
            opidx_t n = (opidx_t) bitset_count (bitset_op);
            if (n == 1) {
                /* the journey advise is completely uniform, we should attempt to
                 * get some diversity by banning the specific operator.
                 */
                opidx_t op = (opidx_t) bitset_next_set_bit (bitset_op, 0);
                set_add_uint8 (req->banned_operators,
                            &req->n_banned_operators,
                            RRRR_MAX_BANNED_OPERATORS,
                            op);
                req->time_cutoff = UNREACHED;
                iterate_origins (router, req, plan, result, n_results);

                /* update our operator list with the output of our last query
                 */
                plan_get_operators (plan, router->tdata, bitset_op);

                /* if we have a result, we don't have to count, we already
                 * know that we have an extra operator here. n++ would suffice.
                 */
                n = (opidx_t) bitset_count (bitset_op);
            }

            if (n > 1) {
                /* the journey advise is pluriform, we observe multiple operators
                 * lets see if it is possible to get uniform results for each of them.
                 */
                uint32_t op = bitset_next_set_bit (bitset_op, 0);

                while (op != BITSET_NONE) {
                    req->n_operators = 0;
                    set_add_uint8 (req->operators, &req->n_operators, RRRR_MAX_FILTERED_OPERATORS, (opidx_t) op);
                    req->time_cutoff = UNREACHED;
                    iterate_origins (router, req, plan, result, n_results);
                    op = bitset_next_set_bit (bitset_op, op + 1);
                }
            }
        }
        bitset_destroy (bitset_op);
    }
    #endif

    free (result);

    plan_sort (plan);
    return true;
}



/* If we would like to estimate all possible departures given
 * a bag of stops in close proximity, we can do so by iterating
 * over this stop lists find the journey patterns at these stops
 * iterate over the active journey patterns for that operating
 * date and return a list of rtime_t candidates.
 */

static int compareRtime(const void *elem1, const void *elem2) {
    return (int) (*(const rtime_t *) elem1) - (*(const rtime_t *) elem2);
}

uint32_t tdata_n_departures_since (tdata_t *td, spidx_t *sps, spidx_t n_stops, rtime_t *result, uint32_t n_results, rtime_t since, rtime_t until) {
    uint32_t n = 0;
    while (n_stops) {
        jpidx_t n_jps;
        jpidx_t *jp_ret;
        n_stops--;
        n_jps = tdata_journey_patterns_for_stop_point (td, sps[n_stops], &jp_ret);

        while (n_jps) {
            jppidx_t n_jpp;
            spidx_t *jpp;
            uint8_t *jpp_a;
            n_jps--;
            /* Implement calendar validation for the journey pattern */
            /* if (router->day_mask & td->journey_pattern_active[n_jps]) */

            jpp = tdata_points_for_journey_pattern(td, jp_ret[n_jps]);
            jpp_a = tdata_stop_point_attributes_for_journey_pattern(td, jp_ret[n_jps]);

            n_jpp = td->journey_patterns[jp_ret[n_jps]].n_stops;
            while (n_jpp) {
                n_jpp--;
                if (jpp_a[n_jpp] & rsa_boarding && jpp[n_jpp] == sps[n_stops]) {
                    jp_vjoffset_t n_vjs = td->journey_patterns[n_jps].n_vjs;
                    while (n_vjs) {
                        vehicle_journey_t *vj;
                        rtime_t arrival;
                        n_vjs--;
                        vj = &td->vjs[n_vjs];
                        arrival = vj->begin_time + td->stop_times[vj->stop_times_offset + n_jpp].arrival;
                        if (arrival >= since && arrival <= until && (n == 0 || result[n - 1] != arrival)) result[n++] = arrival;
                        if (n == n_results) goto full;
                    }
                    /* we can't break here because the same spidx may happen later */
                }
            }
        }
    }

full:
    qsort(result, n, sizeof(rtime_t), compareRtime);
    n = dedupRtime(result, n);
    return n;
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
 * For an arrive_by counter clockwise search, we must make the result
 * clockwise. Only one reversal is required. For the more regular clockwise
 * search, the compression is handled in the first reversal (ccw) and made
 * clockwise in the second reversal.
 *
 * Note that for request that start onboard of a vehicle_journey we do not perform any
 * reversals since there is no wait-time to be compressed
 */
bool router_route_naive_reversal (router_t *router, router_request_t *req, plan_t *plan) {
    uint8_t i;
    uint8_t n_reversals = (uint8_t) (req->arrive_by ? 1 : 2);

    router_reset (router);
    router_request_search_street_network (req, router->tdata);
    if ( ! router_route (router, req) ) {
        return false;
    }

    /*reversal is meaningless/useless in on-board */
    if (req->onboard_journey_pattern == JP_NONE) {
        for (i = 0; i < n_reversals; ++i) {
            if (!router_request_reverse(router, req)) {
                return false;
            }

            router_reset(router);
            if (!router_route(router, req)) {
                return false;
            }

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
    plan_t work_plan;
    uint8_t i_rev;
    uint8_t n_req = 0;
    uint8_t n2_req;

    router_reset (router);
    plan_init (&work_plan);
    router_request_search_street_network (req, router->tdata);

    if ( ! router_route (router, req) ) {
        return false;
    }

    if (req->from_stop_point == ONBOARD){
        /*reversal is meaningless/useless in on-board */
        return router_result_to_plan (plan, router, req);
    }else if ( ! router_result_to_plan (&work_plan, router, req) ) {
        return false;
    }
    /* Copy direct (without street_network itineraries to the result */
    {
        int16_t i_itin = 0;
        for (;i_itin < work_plan.n_itineraries;++i_itin) {
            if (work_plan.itineraries[i_itin].n_legs == 1) {
                plan->itineraries[plan->n_itineraries] = work_plan.itineraries[i_itin];
                ++plan->n_itineraries;
                break;
            }
        }
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
    if (!router_request_reverse_plan (router, &req_storage[i_rev], req_storage, &n_req, &work_plan, i_rev)) {
        return false;
    }

    i_rev++;
    n2_req = n_req;

    for (; i_rev < n_req; ++i_rev) {
        bool reroute = true;
        /* Check if we can skip the third reversal if we rendered a fitting itinerary in the first forward search */
        if (!req_storage[i_rev].arrive_by &&
                req_storage[i_rev].entry.n_points == 1 &&
                req_storage[i_rev].exit.n_points == 1){
            int16_t i_itin;
            for (i_itin = 0; i_itin < work_plan.n_itineraries;++i_itin){
                leg_t first_leg = work_plan.itineraries[i_itin].legs[0];
                leg_t last_leg = work_plan.itineraries[i_itin].legs[work_plan.itineraries[i_itin].n_legs-1];
                if (work_plan.itineraries[i_itin].n_rides == req_storage[i_rev].max_transfers+1 &&
                        first_leg.sp_to == req_storage[i_rev].entry.stop_points[0] &&
                        last_leg.sp_from == req_storage[i_rev].exit.stop_points[0] &&
                        first_leg.t0 == req_storage[i_rev].time &&
                        last_leg.t1 == req_storage[i_rev].time_cutoff){
                    plan->itineraries[plan->n_itineraries] = work_plan.itineraries[i_itin];
                    ++plan->n_itineraries;
                    reroute = false;
                    break;
                }
            }
        }

        if (!reroute) continue;

        router_reset (router);

        if ( ! router_route (router, &req_storage[i_rev]) ) {
            return false;
        }

        if ( ! req_storage[i_rev].arrive_by &&
             ! router_result_to_plan (plan, router, &req_storage[i_rev])) {
            return false;
        }

        if ( ! req->arrive_by && i_rev < n2_req &&
             ! router_request_reverse_all (router, &req_storage[i_rev], req_storage, &n_req, i_rev)) {
            return false;
        }
    }

    return true;
}
