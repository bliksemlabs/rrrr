/* Copyright 2013–2015 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

/* api.c : snippets for using the routerengine */

#include "config.h"
#include "api.h"
#include "street_network.h"
#include "plan_render_text.h"

#ifdef RRRR_DEV
static bool dump_exits_and_entries(router_request_t *req, tdata_t *tdata){
    spidx_t i;
    printf("Entries: \n");
    for (i = 0; i < req->entry.n_points; i++){
        spidx_t sp_index = req->entry.stop_points[i];
        printf("O %d %s %s, %d seconds\n",sp_index,
                tdata_stop_point_id_for_index(tdata, sp_index),
                tdata_stop_point_name_for_index(tdata,sp_index),
                RTIME_TO_SEC(req->entry.durations[i])
        );
    }
    printf("\nExits: \n");
    for (i = 0; i < req->exit.n_points; i++){
        spidx_t sp_index = req->exit.stop_points[i];
        printf("E %d %s %s, %d seconds\n",sp_index,
                tdata_stop_point_id_for_index(tdata, sp_index),
                tdata_stop_point_name_for_index(tdata,sp_index),
                RTIME_TO_SEC(req->exit.durations[i])
        );
    }
    printf("\n");
    return true;
}
#endif

static void
mark_stop_area_in_streetnetwork(spidx_t sa_index, rtime_t duration, tdata_t *tdata, street_network_t *sn){
    spidx_t sp_idx = (spidx_t) tdata->n_stop_points;
    do {
        sp_idx--;
        if (tdata->stop_area_for_stop_point[sp_idx] == sa_index) {
            street_network_mark_duration_to_stop_point(sn, sp_idx, duration);
        }
    } while (sp_idx);
}

static bool search_streetnetwork(router_t *router, router_request_t *req){
    if (req->from_stop_area != STOP_NONE) {
        latlon_t *latlon;
        latlon = tdata_stop_area_coord_for_index(router->tdata, req->from_stop_area);
        streetnetwork_stoppoint_durations(latlon, req->walk_speed, req->walk_max_distance, router->tdata, &req->entry);
        mark_stop_area_in_streetnetwork(req->from_stop_area,0,router->tdata,&req->entry);
    }else if (req->from_stop_point != STOP_NONE){
        latlon_t *latlon;
        latlon = tdata_stop_point_coord_for_index(router->tdata, req->from_stop_point);
        streetnetwork_stoppoint_durations(latlon, req->walk_speed, req->walk_max_distance, router->tdata, &req->entry);
        street_network_mark_duration_to_stop_point(&req->exit, req->from_stop_point, 0);
    }else if (req->from_latlon.lat != 0.0 && req->from_latlon.lon != 0.0){
        streetnetwork_stoppoint_durations(&req->from_latlon, req->walk_speed, req->walk_max_distance, router->tdata, &req->entry);
    }else if (req->onboard_journey_pattern == JP_NONE){
        printf("No coord for entry\n");
        return false;
    }

    if (req->to_stop_area != STOP_NONE) {
        latlon_t *latlon;
        latlon = tdata_stop_area_coord_for_index(router->tdata, req->to_stop_area);
        streetnetwork_stoppoint_durations(latlon, req->walk_speed, req->walk_max_distance, router->tdata, &req->exit);
        mark_stop_area_in_streetnetwork(req->to_stop_area,0,router->tdata,&req->exit);
    }else if (req->to_stop_point != STOP_NONE){
        latlon_t *latlon;
        latlon = tdata_stop_point_coord_for_index(router->tdata, req->to_stop_point);
        streetnetwork_stoppoint_durations(latlon, req->walk_speed, req->walk_max_distance, router->tdata, &req->exit);
        street_network_mark_duration_to_stop_point(&req->exit, req->to_stop_point, 0);
    }else if (req->to_latlon.lat != 0.0 && req->to_latlon.lon != 0.0){
        streetnetwork_stoppoint_durations(&req->to_latlon, req->walk_speed, req->walk_max_distance, router->tdata, &req->exit);
    }else{
        printf("No coord for exit\n");
        return false;
    }
    #ifdef RRRR_DEV
    dump_exits_and_entries(req,router->tdata);
    printf("%d entries, %d exits\n",req->entry.n_points,req->exit.n_points);
    #endif
    return true;
}

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
    search_streetnetwork(router,req);
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
    uint8_t i_rev;
    uint8_t n_req;
    uint8_t n2_req;

    router_reset (router);
    search_streetnetwork(router,req);

    if ( ! router_route (router, req) ) {
        return false;
    }

    if ( ! req->arrive_by &&
         ! router_result_to_plan (plan, router, req) ) {
        return false;
    }

    if (req->from_stop_point == ONBOARD){
        /*reversal is meaningless/useless in on-board */
        return true;
    }

    /* We first add virtual request so we will never do them again */
    for (n_req = 0; n_req < plan->n_itineraries; ++n_req) {
        req_storage[n_req].time = plan->itineraries[n_req].legs[0].t0;
        req_storage[n_req].max_transfers = (uint8_t) (plan->itineraries[n_req].n_rides - 1);
        req_storage[n_req].entry.n_points = 1;
        req_storage[n_req].entry.stop_points[0] = plan->itineraries[n_req].legs[0].sp_to;
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
