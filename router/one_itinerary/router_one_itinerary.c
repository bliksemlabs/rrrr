/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "bitset.h"
#include "plan.h"
#include "util.h"
#include "set.h"
#include "street_network.h"
#include "router_request.h"
#include "router_result.h"
#include "router_one_itinerary.h"

#include <stdlib.h>

static void set_street_network_matching_journey_patterns (tdata_t *tdata, bitset_t *bs, street_network_t *sn, const router_request_t *req) {
    spidx_t n_points;

    bitset_clear (bs);
    n_points = sn->n_points;
    while (n_points) {
        jpidx_t *jp_ret;
        jpidx_t n_jps;
        n_points--;

        n_jps = tdata_journey_patterns_for_stop_point (tdata, sn->stop_points[n_points], &jp_ret);

        while (n_jps) {
            n_jps--;

            if (tdata->journey_pattern_active[jp_ret[n_jps]] & req->day_mask &&
                (req->n_operators == 0 ||
                 set_in_uint8(req->operators, req->n_operators,
                 tdata_operator_idx_for_journey_pattern(tdata, jp_ret[n_jps])))) {
                 bitset_set (bs, jp_ret[n_jps]);
            }
        }
    }
}

static bool get_jpp_for_journey_pattern (tdata_t *tdata, jpidx_t jp_index, street_network_t *sn, jppidx_t *jpp, jpidx_t *sp) {
    spidx_t *sps = tdata_points_for_journey_pattern(tdata, jp_index);
    jppidx_t jpp_idx = (*jpp != JPP_NONE ? *jpp : tdata->journey_patterns[jp_index].n_stops);
    while (jpp_idx) {
        spidx_t n_points = sn->n_points;
        jpp_idx--;

        while (n_points) {
            n_points--;

            if (sps[jpp_idx] == sn->stop_points[n_points]) {
                *jpp = jpp_idx;
                *sp  = sps[jpp_idx];
                goto found;
            }
        }
    }

    return false;

found:
    return true;
}
static void leg_swap(leg_t *leg) {
    struct leg temp = *leg;
    leg->sp_from = temp.sp_to;
    leg->sp_to = temp.sp_from;
    leg->t0 = temp.t1;
    leg->t1 = temp.t0;
#ifdef RRRR_FEATURE_REALTIME_EXPANDED
    leg->jpp0 = temp.jpp1;
    leg->jpp1 = temp.jpp0;
    leg->d0 = temp.d0;
    leg->d1 = temp.d1;
#endif
}

bool router_route_one_trip (tdata_t *tdata, router_request_t *req, plan_t *plan) {
    /* search in journey_patterns_at_stop for both origin and destination
     * for each result maintain either a set (to cross correlate) or
     * a bitmask (and, find results). Results should be filtered based on
     * jp_active and/or vj_active.
     *
     * For the matching patterns returnall searches given the departure time.
     */
    uint32_t jp_index;
    calendar_t day_mask = req->day_mask;
    calendar_t cal_day = 0;

    bitset_t *entry = bitset_new(tdata->n_journey_patterns);
    bitset_t *exit  = bitset_new(tdata->n_journey_patterns);

    router_request_search_street_network (req, tdata);
    street_network_null_duration (&req->entry);

    set_street_network_matching_journey_patterns (tdata, entry, &req->entry, req);
    set_street_network_matching_journey_patterns (tdata, exit,  &req->exit,  req);

    bitset_mask_and (entry, exit);

    while (day_mask >>= 1) cal_day++;

    for (jp_index = bitset_next_set_bit (entry, 0);
         jp_index != BITSET_NONE;
         jp_index = bitset_next_set_bit (entry, jp_index + 1)) {

        vehicle_journey_t *vj;
        calendar_t *vj_masks;
        jp_vjoffset_t vj_offset;
        jppidx_t jpp_from, jpp_to = JPP_NONE;
        spidx_t sp_from, sp_to;

        vj = tdata_vehicle_journeys_in_journey_pattern(tdata, (jpidx_t) jp_index);

        get_jpp_for_journey_pattern (tdata, (jpidx_t) jp_index, (req->arrive_by ? &req->entry : &req->exit), &jpp_to, &sp_to);
        jpp_from = jpp_to; /* make sure the direction is correct */
        if (!get_jpp_for_journey_pattern (tdata, (jpidx_t) jp_index, (req->arrive_by ? &req->exit  : &req->entry), &jpp_from, &sp_from)) {
            continue;
        }

        vj_masks = tdata_vj_masks_for_journey_pattern(tdata, (jpidx_t) jp_index);

        for (vj_offset = 0;
             vj_offset < tdata->journey_patterns[jp_index].n_vjs;
             vj_offset++) {

            rtime_t board_time;
            if ( ! (req->day_mask & vj_masks[vj_offset])) continue;

            board_time  = tdata_stoptime_for_index(tdata, (jpidx_t) jp_index, jpp_from, vj_offset, req->arrive_by) + RTIME_ONE_DAY;
            if (board_time > (req->time + 1800)) break;

            if (board_time >= (req->time)) {
                rtime_t arrival_time = tdata_stoptime_for_index(tdata, (jpidx_t) jp_index, jpp_to, vj_offset, req->arrive_by) + RTIME_ONE_DAY;
                itinerary_t *itin = &plan->itineraries[plan->n_itineraries];
                itin->legs[0].sp_from = sp_from;
                itin->legs[0].sp_to = sp_to;
                itin->legs[0].t0 = board_time;
                itin->legs[0].t1 = arrival_time;
                itin->legs[0].journey_pattern = (jpidx_t) jp_index;
                itin->legs[0].vj = vj_offset;
                itin->legs[0].cal_day = cal_day;
#ifdef RRRR_FEATURE_REALTIME_EXPANDED
                itin->legs[0].jpp0 = jpp_from;
                itin->legs[0].jpp1 = jpp_to;
#endif
                if (req->arrive_by) leg_swap(&itin->legs[0]);
                itin->n_legs = 1;
                plan->n_itineraries++;
                if (plan->n_itineraries >= RRRR_DEFAULT_PLAN_ITIN) goto done;
            }
        }
    }

done:
    bitset_destroy (entry);
    bitset_destroy (exit);

    return true;
}
