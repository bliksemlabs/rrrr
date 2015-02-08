#include "rrrr_types.h"
#include "router_result.h"
#include "router_request.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Reverse the times and stops in a leg.
 * Used for creating arrive-by itineraries.
 */
static void leg_swap (leg_t *leg) {
    struct leg temp = *leg;
    leg->sp_from = temp.sp_to;
    leg->sp_to = temp.sp_from;
    leg->t0 = temp.t1;
    leg->t1 = temp.t0;
    #ifdef RRRR_FEATURE_REALTIME_EXPANDE
    leg->jpp0 = temp.jpp1;
    leg->jpp1 = temp.jpp0;
    #endif
}

static void leg_add_walk (leg_t *leg, router_t *router,
                          uint64_t i_walk, uint64_t i_ride,
                          spidx_t walk_stop_point) {
    /* Walk phase */
    leg->sp_from = router->states_walk_from[i_walk];
    leg->sp_to = walk_stop_point;
    /* Rendering the walk requires already having the ride arrival time */
    leg->t0 = router->states_time[i_ride];
    leg->t1 = router->states_walk_time[i_walk];
    leg->journey_pattern = WALK;
    leg->vj = WALK;
}

#ifdef RRRR_FEATURE_REALTIME_EXPANDED
static void leg_add_ride_delay (leg_t *leg, router_t *router, uint64_t i_ride) {
    journey_pattern_t *jp;
    vehicle_journey_t *vj;
    uint32_t vj_index;

    jp = router->tdata->journey_patterns + router->states_back_journey_pattern[i_ride];
    vj_index = jp->vj_offset + router->states_back_vehicle_journey[i_ride];
    vj = router->tdata->vjs + vj_index;

    if (router->tdata->vj_stoptimes[vj_index] &&
        router->tdata->stop_times[vj->stop_times_offset + router->states_journey_pattern_point[i_ride]].arrival != UNREACHED) {

        leg->d0 = RTIME_TO_SEC_SIGNED(router->tdata->vj_stoptimes[vj_index][router->states_back_journey_pattern_point[i_ride]].departure) - RTIME_TO_SEC_SIGNED(router->tdata->stop_times[vj->stop_times_offset + router->states_back_journey_pattern_point[i_ride]].departure + vj->begin_time);
        leg->d1 = RTIME_TO_SEC_SIGNED(router->tdata->vj_stoptimes[vj_index][router->states_journey_pattern_point[i_ride]].arrival) - RTIME_TO_SEC_SIGNED(router->tdata->stop_times[vj->stop_times_offset + router->states_journey_pattern_point[i_ride]].arrival + vj->begin_time);
    } else {
        leg->d0 = 0;
        leg->d1 = 0;
    }

    leg->jpp0 = router->states_back_journey_pattern_point[i_ride];
    leg->jpp1 = router->states_journey_pattern_point[i_ride];
}
#endif

static void leg_add_ride (leg_t *leg, router_t *router,
                          uint64_t i_ride, spidx_t ride_stop_point) {
    leg->sp_from = router->states_ride_from[i_ride];
    leg->sp_to = ride_stop_point;
    leg->t0 = router->states_board_time[i_ride];
    leg->t1 = router->states_time[i_ride];
    leg->journey_pattern = router->states_back_journey_pattern[i_ride];
    leg->vj = router->states_back_vehicle_journey[i_ride];

    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    leg_add_ride_delay (leg, router, i_ride);
    #endif
}

/* Checks charateristics that should be the same for all vj plans produced
 * by this router:
 * All stops should chain, all times should be increasing, all waits
 * should be at the ends of walk legs, etc.
 * Returns true if any of the checks fail, false if no problems are detected.
 */
static bool check_plan_invariants (plan_t *plan) {
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
                    if (leg->journey_pattern != WALK) fprintf(stderr, "even numbered leg %d has journey_pattern %d not WALK.\n", i_leg, leg->journey_pattern);
                    fail = true;
                } else {
                    if (leg->journey_pattern == WALK) fprintf(stderr, "odd numbered leg %d has journey_pattern WALK.\n", i_leg);
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
                    if (leg->journey_pattern == WALK && leg->t0 != prev_leg->t1) {
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

void router_result_sort (plan_t *plan) {
    qsort(&plan->itineraries, plan->n_itineraries, sizeof(itinerary_t), compareItineraries);
}

bool router_result_to_plan (plan_t *plan, router_t *router, router_request_t *req) {
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

        /* the slot in which record a leg,
         * reversing them for forward vehicle_journey's
         */
        leg_t *l = itin->legs;

        /* Work backward from the target to the origin */
        spidx_t sp_index = router->target;

        /* signed int because we will be decreasing */
        int16_t j_transfer;

        i_state = (((uint64_t) i_transfer) * router->tdata->n_stop_points) + sp_index;

        /* skip rounds that were not reached */
        if (router->states_walk_time[i_state] == UNREACHED) continue;

        itin->n_rides = i_transfer + 1;

        /* always same number of legs for same number of transfers */
        itin->n_legs = itin->n_rides * 2 + 1;

        if ( ! req->arrive_by) l += itin->n_legs - 1;

        /* Follow the chain of states backward */
        for (j_transfer = i_transfer; j_transfer >= 0; --j_transfer) {
            uint64_t i_walk, i_ride;
            spidx_t walk_stop_point;
            spidx_t ride_stop_point;

            i_state = ((uint64_t) j_transfer) * router->tdata->n_stop_points;

            if (sp_index > router->tdata->n_stop_points) {
                fprintf (stderr, "ERROR: stop_point idx %d out of range.\n", sp_index);
                return false;
            }

            /* Walk phase */
            i_walk = i_state + sp_index;
            if (router->states_walk_time[i_walk] == UNREACHED) {
                fprintf (stderr, "ERROR: stop_point idx %d was unreached by walking.\n", sp_index);
                return false;
            }
            walk_stop_point = sp_index;

            /* follow the chain of states backward */
            sp_index = router->states_walk_from[i_walk];

            /* Ride phase */
            i_ride = i_state + sp_index;
            if (router->states_time[i_ride] == UNREACHED) {
                fprintf (stderr, "ERROR: sp %d was unreached by riding.\n", sp_index);
                return false;
            }
            ride_stop_point = sp_index;
            /* follow the chain of states backward */
            sp_index = router->states_ride_from[i_ride];

            /* Walk phase */
            leg_add_walk(l, router, i_walk, i_ride, walk_stop_point);

            if (req->arrive_by) leg_swap (l);
            l += (req->arrive_by ? 1 : -1); /* next leg */

            /* Ride phase */
            leg_add_ride (l, router, i_ride, ride_stop_point);

            if (req->arrive_by) leg_swap (l);
            l += (req->arrive_by ? 1 : -1);   /* next leg */

        }
        if (req->onboard_journey_pattern_vjoffset != NONE) {
            if (!req->arrive_by) {
                /* Results starting on board do not have an initial walk leg. */
                l->sp_from = l->sp_to = ONBOARD;
                l->t0 = l->t1 = req->time;
                l->journey_pattern = l->vj = WALK;
                l += 1; /* move back to first transit leg */
                l->sp_from = ONBOARD;
                l->t0 = req->time;
            } else {
                #ifdef RRRR_DEBUG
                fprintf(stderr, "We observed an onboard departure with an arrive by.\n");
                #endif
                return false;
            }
        } else {
            /* The initial walk leg leading out of the search origin.
             * This is inferred, not stored explicitly.
             */
            spidx_t origin_stop_point = (req->arrive_by ? req->to_stop_point : req->from_stop_point);
            rtime_t duration;
            leg_t *prev;

            l->sp_from = origin_stop_point;
            l->sp_to = sp_index;

            /* Compress out the wait time from s1 to s0
             */
            prev = (l - (req->arrive_by ? 1 : -1));
            l->t1 = (req->arrive_by ? prev->t1 : prev->t0);
            duration = transfer_duration (router->tdata, req, l->sp_from, l->sp_to);
            l->t0 = l->t1 + (req->arrive_by ? +duration : -duration);
            l->journey_pattern = WALK;
            l->vj = WALK;
            if (req->arrive_by) leg_swap (l);
        }
        /* Move to the next itinerary in the plan. */
        plan->n_itineraries += 1;
        itin += 1;
    }
    return check_plan_invariants (plan);
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

    if (!router_result_to_plan (&plan, router, req)) {
        return 0;
    }

    return render (&plan, router->tdata, buf, buflen);
}

