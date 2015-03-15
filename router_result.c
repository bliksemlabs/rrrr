#include "rrrr_types.h"
#include "router_result.h"
#include "street_network.h"
#include <stdio.h>
#include <stdlib.h>

void router_result_init_plan(plan_t *plan) {
    plan->n_itineraries = 0;
}

void router_result_init_itinerary(itinerary_t *itin) {
    itin->n_legs = 0;
    itin->n_rides = 0;
}
/* Reverse the times and stops in a leg.
 * Used for creating arrive-by itineraries.
 */
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


#ifdef RRRR_FEATURE_REALTIME_EXPANDED

static void leg_add_ride_delay(leg_t *leg, router_t *router, int64_t i_ride) {
    journey_pattern_t *jp;
    vehicle_journey_t *vj;
    vjidx_t vj_index;

    jp = router->tdata->journey_patterns + router->states_back_journey_pattern[i_ride];
    vj_index = jp->vj_index + router->states_back_vehicle_journey[i_ride];
    vj = router->tdata->vjs + vj_index;

    if (router->tdata->vj_stoptimes[vj_index] &&
            router->tdata->stop_times[vj->stop_times_offset + router->states_journey_pattern_point[i_ride]].arrival != UNREACHED) {

        leg->d0 = (int16_t) (RTIME_TO_SEC_SIGNED(router->tdata->vj_stoptimes[vj_index][router->states_back_journey_pattern_point[i_ride]].departure) - RTIME_TO_SEC_SIGNED(router->tdata->stop_times[vj->stop_times_offset + router->states_back_journey_pattern_point[i_ride]].departure + vj->begin_time));
        leg->d1 = (int16_t) (RTIME_TO_SEC_SIGNED(router->tdata->vj_stoptimes[vj_index][router->states_journey_pattern_point[i_ride]].arrival) - RTIME_TO_SEC_SIGNED(router->tdata->stop_times[vj->stop_times_offset + router->states_journey_pattern_point[i_ride]].arrival + vj->begin_time));
    } else {
        leg->d0 = 0;
        leg->d1 = 0;
    }

}

#endif

static void leg_add_ride(itinerary_t *itin, leg_t *leg, router_t *router, router_request_t *req,
        int64_t i_state, spidx_t rid_from_stoppoint) {
    int64_t i_ride = i_state + rid_from_stoppoint;
    leg->sp_from = router->states_ride_from[i_ride];
    leg->sp_to = rid_from_stoppoint;
    leg->t0 = router->states_board_time[i_ride];
    leg->t1 = router->states_time[i_ride];
    leg->journey_pattern = router->states_back_journey_pattern[i_ride];
    leg->vj = router->states_back_vehicle_journey[i_ride];
#ifdef RRRR_FEATURE_REALTIME_EXPANDED
    leg->jpp0 = router->states_back_journey_pattern_point[i_ride];
    leg->jpp1 = router->states_journey_pattern_point[i_ride];
    { /* Infer the serviceday using the time */
        int8_t i_serviceday = 0;
        leg->cal_day = 0;
        for (; i_serviceday < router->n_servicedays; ++i_serviceday){
            if (leg->t0 == router->servicedays[i_serviceday].midnight +
                    tdata_stoptime_for_index(router->tdata, leg->journey_pattern, leg->jpp0, leg->vj, req->arrive_by)){
                calendar_t day_mask = router->servicedays[i_serviceday].mask;
                while (day_mask >>= 1) {
                    leg->cal_day++;
                }
                break;
            }
        }
    }
    leg_add_ride_delay(leg, router, i_ride);
#endif
    if (req->arrive_by) leg_swap(leg);
    ++itin->n_legs;
}

/* Checks charateristics that should be the same for all vj plans produced
 * by this router:
 * All stops should chain, all times should be increasing, all waits
 * should be at the ends of walk legs, etc.
 * Returns true if any of the checks fail, false if no problems are detected.
 */
static bool check_plan_invariants(plan_t *plan) {
    itinerary_t *prev_itin = NULL;
    rtime_t prev_target_time = UNREACHED;
    uint8_t i_itinerary;
    bool fail = false;

    /* Loop over all itineraries in this plan. */
    for (i_itinerary = 0; i_itinerary < plan->n_itineraries; ++i_itinerary) {
        itinerary_t *itin = plan->itineraries + i_itinerary;
        if (itin->n_legs < 1) {
            fprintf(stderr, "itinerary contains no legs.\n");
            fail = true;
        } else {
            /* Itinarary has at least one leg. Grab its first and last leg. */
            leg_t *leg0 = itin->legs;
            leg_t *legN = itin->legs + (itin->n_legs - 1);
            /* Itineraries should be Pareto-optimal.
             * Increase in number of rides implies improving arrival time.
             */
            rtime_t target_time = plan->req.arrive_by ? leg0->t0 : legN->t1;
            if (prev_itin != NULL) {
                if (itin->n_legs <= prev_itin->n_legs) {
                    fprintf(stderr, "itineraries do not have strictly increasing numbers of legs: %d, %d.\n", prev_itin->n_legs, itin->n_legs);
                    fail = true;
                }
                if (plan->req.arrive_by ? target_time <= prev_target_time : target_time >= prev_target_time) {
                    fprintf(stderr, "itineraries do not have strictly improving target times: %d, %d.\n", prev_target_time, target_time);
                    fail = true;
                }
            }
            prev_target_time = target_time;
            prev_itin = itin;
            /* Check that itinerary does indeed connect the places
             * in the request.
             */
            if (leg0->sp_from != plan->req.from_stop_point) {
                fprintf(stderr, "itinerary does not begin at from location.\n");
                fail = true;
            }
            if (legN->sp_to != plan->req.to_stop_point) {
                fprintf(stderr, "itinerary does not end at to location.\n");
                fail = true;
            }
            /* Check that the itinerary respects the depart after or
             * arrive-by criterion.
             */
            /* finish when rtimes are in requests
               if (plan->req.arrive_by) {
               if (itin->legs[itin->n_legs - 1].s1 > plan->req.time)...
               } else {
               if (itin->legs[0].s0 < plan->req.time)...
               }
               */
            /* All itineraries are composed of ride-walk pairs, prefixed by a single walk leg. */
            if (itin->n_legs % 2 != 1) {
                fprintf(stderr, "itinerary has an inexplicable (even) number of legs: %d\n", itin->n_legs);
                fail = true;
            }
        }
        {
            /* Check per-leg invariants within each itinerary. */
            leg_t *prev_leg = NULL;
            uint8_t i_leg;
            for (i_leg = 0; i_leg < itin->n_legs; ++i_leg) {
                leg_t *leg = itin->legs + i_leg;
                if (i_leg % 2 == 0) {
                    if (leg->journey_pattern < WALK) fprintf(stderr, "even numbered leg %d has journey_pattern %d not WALK.\n", i_leg, leg->journey_pattern);
                    fail = true;
                } else {
                    if (leg->journey_pattern >= WALK) fprintf(stderr, "odd numbered leg %d has journey_pattern WALK.\n", i_leg);
                    fail = true;
                }
                if (leg->t1 < leg->t0) {
                    fprintf(stderr, "non-increasing times within leg %d: %d, %d\n", i_leg, leg->t0, leg->t1);
                    fail = true;
                }
                if (i_leg > 0) {
                    if (leg->sp_from != prev_leg->sp_to) {
                        fprintf(stderr, "legs do not chain: leg %d begins with stop_point %d, previous leg ends with stop_point %d.\n", i_leg, leg->sp_from, prev_leg->sp_to);
                        fail = true;
                    }
                    if (leg->journey_pattern >= WALK && leg->t0 != prev_leg->t1) {
                        /* This will fail unless reversal is being performed */
#if 0
                        fprintf(stderr, "walk leg does not immediately follow ride: leg %d begins at time %d, previous leg ends at time %d.\n", l, leg->t0, prev_leg->t1);
                        fail = true;
                        #endif
                    }
                    if (leg->t0 < prev_leg->t1) {
                        fprintf(stderr, "itin %d: non-increasing times between legs %d and %d: %d, %d\n",
                                i_itinerary, i_leg - 1, i_leg, prev_leg->t1, leg->t0);
                        fail = true;
                    }
                }
                prev_leg = leg;
            } /* End for (legs) */
        }
    } /* End for (itineraries) */
    return fail;
}

static int
compareItineraries(const void *elem1, const void *elem2) {
    itinerary_t *i1 = (itinerary_t *) elem1;
    itinerary_t *i2 = (itinerary_t *) elem2;

    return ((i1->legs[0].t0 - i2->legs[0].t0) << 3) +
            i1->legs[i1->n_legs - 1].t1 - i2->legs[i2->n_legs - 1].t1;
}

void router_result_sort(plan_t *plan) {
    qsort(&plan->itineraries, plan->n_itineraries, sizeof(itinerary_t), compareItineraries);
}

static void leg_add_target(itinerary_t *itin, leg_t *leg, router_t *router, router_request_t *req,
        int64_t i_state, street_network_t *target, int32_t i_target) {
    spidx_t sp_index = target->stop_points[i_target];
    /* Target to first transit with streetnetwork phase */
    leg->sp_from = sp_index;
    leg->sp_to = req->arrive_by ? req->from_stop_point : req->to_stop_point;

    /* Rendering the walk requires already having the ride arrival time */
    leg->t0 = router->states_time[i_state + sp_index];
    leg->t1 = req->arrive_by ? leg->t0 - target->durations[i_target] :
                               leg->t0 + target->durations[i_target];
    leg->journey_pattern = STREET;
    leg->vj = STREET;
    if (req->arrive_by) leg_swap(leg);
    ++itin->n_legs;
}

static void leg_add_origin(itinerary_t *itin, leg_t *leg, router_t *router, router_request_t *req,
        int64_t i_state, street_network_t *origin, int32_t i_origin, spidx_t board_sp) {
    spidx_t sp_index = origin->stop_points[i_origin];
    /* Target to first transit with streetnetwork phase */
    leg->sp_from = req->arrive_by ? req->to_stop_point : req->from_stop_point;
    leg->sp_to = sp_index;

    /* Rendering the walk requires already having the ride arrival time */
    leg->t1 = router->states_board_time[i_state + board_sp];
    leg->t0 = req->arrive_by ? leg->t1 + origin->durations[i_origin] :
                               leg->t1 - origin->durations[i_origin];
    leg->journey_pattern = STREET;
    leg->vj = STREET;
    if (req->arrive_by) leg_swap(leg);
    ++itin->n_legs;
}


static void leg_add_direct(leg_t *leg, router_request_t *req, rtime_t duration) {
    /* Target to first transit with streetnetwork phase */
    leg->sp_from = req->arrive_by ? req->to_stop_point : req->from_stop_point;
    leg->sp_to = req->arrive_by ? req->from_stop_point : req->to_stop_point;

    /* Rendering the walk requires already having the ride arrival time */
    leg->t0 = req->time;
    leg->t1 = leg->t0 + duration;
    leg->journey_pattern = STREET;
    leg->vj = STREET;
    if (req->arrive_by) leg_swap(leg);

}

static void leg_add_vj_interline(itinerary_t *itin, leg_t *leg, router_t *router, router_request_t *req,
        uint64_t i_state, spidx_t board_sp, spidx_t from_sp, spidx_t to_sp) {
    leg->sp_from = from_sp;
    leg->sp_to = to_sp;

    /* Rendering the walk requires already having the ride arrival time */
    leg->t0 = router->states_time[i_state + to_sp];
    leg->t1 = router->states_board_time[i_state + board_sp];
    leg->journey_pattern = STAY_ON;
    leg->vj = STAY_ON;
    if (req->arrive_by) leg_swap(leg);
    ++itin->n_legs;
}

static void leg_add_transfer(itinerary_t *itin, leg_t *leg, router_t *router, router_request_t *req,
        int64_t i_state,
        spidx_t walk_end_stop_point) {
    /* Walk phase */
    leg->sp_from = router->states_walk_from[i_state + walk_end_stop_point];
    leg->sp_to = walk_end_stop_point;
    /* Rendering the walk requires already having the ride arrival time */
    leg->t0 = router->states_time[i_state + leg->sp_from];
    leg->t1 = router->states_walk_time[i_state + walk_end_stop_point];
    leg->journey_pattern = WALK;
    leg->vj = WALK;
    if (req->arrive_by) leg_swap(leg);
    ++itin->n_legs;
}

void leg_add_onboard(itinerary_t *itin, leg_t *leg, router_request_t *req){
    leg->sp_from = leg->sp_to = ONBOARD;
    leg->t0 = leg->t1 = req->time;
    leg->journey_pattern = leg->vj = STREET;
    leg->sp_from = ONBOARD;
    leg->t0 = req->time;
    ++itin->n_legs;
}

void reverse_legs(itinerary_t *itin){
    leg_t legs[MAX_LEGS];
    int32_t i_leg;
    for (i_leg = 0; i_leg < itin->n_legs; ++i_leg){
        legs[i_leg] = itin->legs[itin->n_legs-i_leg-1];
    }
    for (i_leg = 0; i_leg < itin->n_legs; ++i_leg){
        itin->legs[i_leg] = legs[i_leg];
    }
}

bool render_itinerary(router_t *router, router_request_t *req, itinerary_t *itin,
        uint8_t round, street_network_t *target, spidx_t i_target) {
    jppidx_t jp_index;
    jp_vjoffset_t vj_offset;
    street_network_t *origin = req->arrive_by ? &req->exit : &req->entry;
    spidx_t sp_index = target->stop_points[i_target];
    rtime_t duration_target = target->durations[i_target];
    spidx_t current_sp = sp_index;
    int64_t i_state = (((uint64_t) round) * router->tdata->n_stop_points);

    router_result_init_itinerary(itin);
    
    if (router->states_time[i_state + sp_index] == UNREACHED) {
        /* Render a itinerary that does not touch the transit network */
        leg_add_direct(itin->legs + itin->n_legs, req, duration_target);
        return true;
    } else {
        /* Append the leg between the target and the first vehicle_journey in the itinerary*/
        leg_add_target(itin, itin->legs + itin->n_legs, router, req, i_state, target, i_target);
    }

    /* Start with the first vehicle_journey and then navigate back through the states to an origin */
    jp_index = router->states_back_journey_pattern[i_state + sp_index];
    vj_offset = router->states_back_vehicle_journey[i_state + sp_index];
    ++itin->n_rides;
    while (itin->n_legs < 100) {
        spidx_t board_sp = current_sp;
        rtime_t current_time;
#ifdef RRRR_INTERLINE_DEBUG
        printf("JP is is for line %s\n", tdata_line_id_for_journey_pattern(router->tdata, jp_index));
        printf("JP_index %d, Destination %s\n", jp_index, tdata_headsign_for_journey_pattern(router->tdata, jp_index));
        printf("Trip_id %s\n", tdata_vehicle_journey_id_for_jp_vj_offset(router->tdata, jp_index, vj_offset));
        printf("Add_ride_new From Sp_index %d %s\n", sp_index, tdata_stop_point_name_for_index(router->tdata, current_sp));
#endif
        leg_add_ride(itin, itin->legs + itin->n_legs, router, req, i_state, current_sp);
#ifdef RRRR_INTERLINE_DEBUG
        printf("Ride from %s [%d] to %s [%d]\n",
                tdata_stop_point_name_for_index(router->tdata,
                        (itin->legs + itin->n_legs-1)->sp_from),
                (itin->legs + itin->n_legs-1)->sp_from,
                tdata_stop_point_name_for_index(router->tdata,
                        (itin->legs + itin->n_legs-1)->sp_to),
                (itin->legs + itin->n_legs-1)->sp_to);
#endif

        current_time = router->states_board_time[i_state + current_sp];
        current_sp = router->states_ride_from[i_state + current_sp];
#ifdef RRRR_INTERLINE_DEBUG
        {
            char time32[32];
            char time33[32];
            printf("Check interline  at Sp_index %d %s current_time %s, states_walk_time %s, prev_round states_walk_time %s"
                    "\n", sp_index, tdata_stop_point_name_for_index(router->tdata, current_sp),btimetext(current_time,time32),
                    btimetext(router->states_walk_time[i_state + current_sp],time32),btimetext(router->states_walk_time[i_state + current_sp + router->tdata->n_stop_points],time33));

            printf("current_time %d, states_walk_time %d\n",
                    current_time,
                    router->states_walk_time[i_state + current_sp]);

            printf("current_time %s, states_walk_time %s\n",
                    btimetext(current_time, time32),
                    btimetext(router->states_walk_time[i_state + current_sp], time33));

            printf("current_sp %d, states_walk_from %d (%s) JP_back %d\n",
                    current_sp,
                    router->states_walk_from[i_state + current_sp],
                    tdata_stop_point_name_for_index(router->tdata, current_sp),
                    router->states_back_journey_pattern[i_state + current_sp]);
        }
#endif


        if (street_network_duration(current_sp, origin) != UNREACHED) {
            /* Origin reached, completing the itinerary */
            int32_t i_origin = 0;
            for (;i_origin < origin->n_points; i_origin++){
                if (origin->stop_points[i_origin] == current_sp)
                    break;
            }
            leg_add_origin(itin, itin->legs + itin->n_legs, router, req,
                    i_state, origin, i_origin, board_sp);
            if (!req->arrive_by) reverse_legs(itin);
            return true;
        }
        else if (req->onboard_journey_pattern != JP_NONE &&
                 req->onboard_journey_pattern == jp_index &&
                 req->onboard_journey_pattern_vjoffset == vj_offset){
            /* Change the start-position of the first transit leg to ONBOARD */
            (itin->legs + itin->n_legs-1)->sp_from = ONBOARD;
            leg_add_onboard(itin, itin->legs + itin->n_legs, req);
            if (!req->arrive_by) reverse_legs(itin);
            return true;
        }
        /* Check whether we arrived on this stop_point before the time we could have walked there
         * This implies a vehicle_journey interline */
        else if (round == 0 ||
                router->states_walk_time[i_state + current_sp - router->tdata->n_stop_points] == UNREACHED ||
                (req->arrive_by ? current_time > router->states_walk_time[i_state + current_sp  - router->tdata->n_stop_points] :
                                  current_time < router->states_walk_time[i_state + current_sp  - router->tdata->n_stop_points])) {
#ifdef RRRR_INTERLINE_DEBUG
            printf("current_sp %d, states_walk_from %d (%s) <%d>\n",
                    current_sp,
                    router->states_walk_from[i_state + current_sp],
                    tdata_stop_point_name_for_index(router->tdata, current_sp),
                    router->states_walk_from[i_state + current_sp] != current_sp
            );
#endif

            journey_pattern_t *jp = &router->tdata->journey_patterns[jp_index];
            vehicle_journey_ref_t *vj_interline = req->arrive_by ?
                    &router->tdata->vehicle_journey_transfers_forward[jp->vj_index + vj_offset] :
                    &router->tdata->vehicle_journey_transfers_backward[jp->vj_index + vj_offset];
            if (vj_interline->jp_index != JP_NONE) {
                jppidx_t prev_jp_index = vj_interline->jp_index;
                jp_vjoffset_t prev_vj_offset = vj_interline->vj_offset;
                journey_pattern_t *prev_jp = &router->tdata->journey_patterns[prev_jp_index];
                spidx_t last_sp_of_prev_vj = req->arrive_by ? router->tdata->journey_pattern_points[prev_jp->journey_pattern_point_offset] :
                        router->tdata->journey_pattern_points[prev_jp->journey_pattern_point_offset + prev_jp->n_stops - 1];
                if (router->states_time[i_state + last_sp_of_prev_vj] != UNREACHED ||
                        router->states_back_journey_pattern[i_state + last_sp_of_prev_vj] == prev_jp_index ||
                        router->states_back_vehicle_journey[i_state + last_sp_of_prev_vj] == prev_vj_offset) {
                    leg_add_vj_interline(itin, itin->legs + itin->n_legs, router, req, i_state, board_sp, current_sp, last_sp_of_prev_vj);
                    current_sp = last_sp_of_prev_vj;
                    jp_index = prev_jp_index;
                    vj_offset = prev_vj_offset;
                    continue;
                }
            }
        }
        {
            i_state -= router->tdata->n_stop_points;
            leg_add_transfer(itin, itin->legs + itin->n_legs, router, req, i_state, current_sp);
#ifdef RRRR_INTERLINE_DEBUG
            printf("Walk from %s [%d] to %s [%d]\n",
                    tdata_stop_point_name_for_index(router->tdata,
                            (itin->legs + itin->n_legs-1)->sp_from),
                    (itin->legs + itin->n_legs-1)->sp_from,
                    tdata_stop_point_name_for_index(router->tdata,
                            (itin->legs + itin->n_legs-1)->sp_to),
                    (itin->legs + itin->n_legs-1)->sp_to);
#endif
            current_sp = router->states_walk_from[i_state + current_sp];
            ++itin->n_rides;
            --round;
            if (router->states_time[i_state + current_sp] == UNREACHED){
                fprintf(stderr,"Transfer to unreached location\n");
                return false;
            }
            if (street_network_duration(current_sp,target) < duration_target){
                /* This journey is sub-optimal as it passes a more optimal target */
                return false;
            }
            jp_index = router->states_back_journey_pattern[i_state + current_sp];
            vj_offset = router->states_back_vehicle_journey[i_state + current_sp];
        }
    }
    fprintf(stderr, "Something went terribly wrong during rendering\n");
    return false;
}

/* Returns whether given round n, the given target (i_target) is the best target, or that a different target has a
 * better time for the same vehicle journey. Optionally (optimizeOnLessStreet) it also checks whether is is the target
 * with the least distance/duration on the street_network.
 */
bool best_target_for_jp_vj(router_t *router, router_request_t *req, uint64_t i_state, street_network_t *target, spidx_t i_target, bool optimizeOnLessStreet) {
    spidx_t sp_index = target->stop_points[i_target];
    jpidx_t jp_index = router->states_back_journey_pattern[i_state + sp_index];
    jp_vjoffset_t vj_index = router->states_back_vehicle_journey[i_state + sp_index];

    rtime_t best_sn_duration = target->durations[i_target];
    spidx_t target_least_sn_duration = i_target;

    rtime_t best_time = router->states_time[i_state + sp_index] + best_sn_duration;
    spidx_t target_best = i_target;
    int32_t i_otarget = target->n_points;
    while (i_otarget) {
        rtime_t duration;
        --i_otarget;
        sp_index = target->stop_points[i_otarget];
        duration = target->durations[i_otarget];
        if (i_target == i_otarget || router->states_time[i_state + sp_index] == UNREACHED ||
                router->states_back_journey_pattern[i_state + sp_index] != jp_index ||
                router->states_back_vehicle_journey[i_state + sp_index] != vj_index) {
            continue;
        }
        if (optimizeOnLessStreet && target->durations[i_otarget] < best_sn_duration) {
            best_sn_duration = target_least_sn_duration;
            target_least_sn_duration = (spidx_t) i_otarget;
        }
        if (req->arrive_by ? router->states_time[i_state + sp_index] - duration > best_time :
                router->states_time[i_state + sp_index] + duration < best_time) {
            best_time = req->arrive_by ? router->states_time[i_state + sp_index] - duration :
                    router->states_time[i_state + sp_index] + duration;
            target_best = (spidx_t) i_otarget;
        }
    }
    return i_target == target_best || (optimizeOnLessStreet && i_target == target_least_sn_duration);
}

/* Get the best end-time given street_netwerk and i_state */
rtime_t best_time_in_round(router_t *router, router_request_t *req, uint64_t i_state, street_network_t *sn) {
    int32_t i_target;
    rtime_t best_time = (rtime_t) (req->arrive_by ? 0 : UNREACHED);
    for (i_target = 0; i_target < sn->n_points; ++i_target) {
        spidx_t sp_index = sn->stop_points[i_target];
        rtime_t duration = sn->durations[i_target];
        if (req->arrive_by ? (router->states_time[i_state + sp_index] != UNREACHED &&
                              router->states_time[i_state + sp_index] - duration > best_time) :
                             router->states_time[i_state + sp_index] + duration < best_time) {
            best_time = req->arrive_by ? router->states_time[i_state + sp_index] - duration :
                    router->states_time[i_state + sp_index] + duration;
        }
    }
    return (rtime_t) (best_time == 0 ? UNREACHED : best_time);
}

bool router_result_to_plan(plan_t *plan, router_t *router, router_request_t *req) {
    itinerary_t *itin;
    uint8_t i_transfer;
    /* copy the request into the plan for use in rendering */
    plan->req = *req;
    itin = &plan->itineraries[plan->n_itineraries];

    /* Loop over the rounds to get ending states of itineraries
     * using different numbers of vehicles
     */
    for (i_transfer = 0; i_transfer < RRRR_DEFAULT_MAX_ROUNDS; ++i_transfer) {
        /* Work backward from the target to the origin */
        uint64_t i_state;
        spidx_t i_target;
        rtime_t best_time_round;
        street_network_t *target = req->arrive_by ? &req->entry : &req->exit;
        if (target->n_points == 0) {
            printf("No targets\n");
            continue;
        }
        i_state = (((uint64_t) i_transfer) * router->tdata->n_stop_points);
        best_time_round = best_time_in_round(router, req, i_state, target);
        if (best_time_round == UNREACHED)
            continue; /* No targets reached with this number of transfers */

        /* Scan targets for optimal itinaries with i_transfer transfers */
        for (i_target = 0; i_target < target->n_points; ++i_target) {
            rtime_t duration = target->durations[i_target];
            spidx_t sp_index = target->stop_points[i_target];

            /* Skip the targets which were not reached by a vhicle in the round or have worse times than the best_time */
            if (router->states_time[i_state + sp_index] == UNREACHED ||
                    (req->arrive_by ? router->states_time[i_state + sp_index] - duration < best_time_round :
                                      router->states_time[i_state + sp_index] + duration > best_time_round) ||
                    !best_target_for_jp_vj(router, req, i_state, target, i_target, false)) {
                continue;
            }

            /* Work backward from the targets to the origin */
            if (render_itinerary(router, req, itin, i_transfer, target, i_target)) {
                /* Move to the next itinerary in the plan. */
                plan->n_itineraries += 1;
                itin += 1;
            }else{
                printf("Itin render fault\n");
            }
        };
    }
    return check_plan_invariants(plan);
}

/* After routing, call to convert the router state into a readable list of
 * itinerary legs. Returns the number of bytes written to the buffer.
 */
uint32_t
router_result_dump(router_t *router, router_request_t *req,
        uint32_t(*render)(plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen),
        char *buf, uint32_t buflen) {
    plan_t plan;
    plan.n_itineraries = 0;

    if (!router_result_to_plan(&plan, router, req)) {
        return 0;
    }

    return render(&plan, router->tdata, buf, buflen);
}
