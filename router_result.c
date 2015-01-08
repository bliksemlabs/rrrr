#include "rrrr_types.h"
#include "router_result.h"
#include "router_request.h"
#include <stdio.h>
#include <string.h>

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

bool router_result_to_plan (struct plan *plan, router_t *router, router_request_t *req) {
    itinerary_t *itin;
    uint8_t i_transfer;
    plan->n_itineraries = 0;
    /* copy the request into the plan for use in rendering */
    plan->req = *req;
    itin = plan->itineraries;

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
        if (req->onboard_journey_pattern_offset != NONE) {
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

            l->sp_from = origin_stop_point;
            l->sp_to = sp_index;
            /* It would also be possible to work from s1 to s0 and compress
             * out the wait time.
             */
            l->t0 = router->states_time[origin_stop_point];
            duration = transfer_duration (router->tdata, req, l->sp_from, l->sp_to);
            l->t1 = l->t0 + (req->arrive_by ? -duration : +duration);
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

#ifdef RRRR_FEATURE_REALTIME_ALERTS
static void
leg_add_alerts (leg_t *leg, tdata_t *tdata, time_t date, char **alert_msg) {
    size_t i_entity;
    uint64_t t0 = date + RTIME_TO_SEC(leg->t0 - RTIME_ONE_DAY);
    uint64_t t1 = date + RTIME_TO_SEC(leg->t1 - RTIME_ONE_DAY);
    for (i_entity = 0; i_entity < tdata->alerts->n_entity; ++i_entity) {
        if (tdata->alerts->entity[i_entity] &&
            tdata->alerts->entity[i_entity]->alert) {
            TransitRealtime__Alert *alert = tdata->alerts->entity[i_entity]->alert;

            if (alert->n_active_period > 0) {
                size_t i_active_period;
                for (i_active_period = 0; i_active_period < alert->n_active_period; ++i_active_period) {
                    TransitRealtime__TimeRange *active_period = alert->active_period[i_active_period];
                    size_t i_informed_entity;

                    if (active_period->start >= t1 || active_period->end <= t0) continue;

                    for (i_informed_entity = 0; i_informed_entity < alert->n_informed_entity; ++i_informed_entity) {
                        TransitRealtime__EntitySelector *informed_entity = alert->informed_entity[i_informed_entity];

                        if ( ( (!informed_entity->route_id) || ((uint32_t) *(informed_entity->route_id) == leg->journey_pattern) ) &&
                            ( (!informed_entity->stop_id)  || (
                                ((uint32_t) *(informed_entity->stop_id) == leg->sp_from && active_period->start <= t0 && active_period->end >= t0 ) ||
                                ((uint32_t) *(informed_entity->stop_id) == leg->sp_to   && active_period->start <= t1 && active_period->end >= t1 )
                            ) ) &&
                            ( (!informed_entity->trip)     || (!informed_entity->trip->trip_id) || ((uint32_t) *(informed_entity->trip->trip_id) == leg->vj) )
                            /* TODO: need to have the start date for a trip_id for informed_entity->trip->start_date */
                        ) {
                            *alert_msg = alert->header_text->translation[0]->text;
                        }

                        /* TODO: theoretically we could have multiple alert messages */
                        if (*alert_msg) return;
                    }
                }
            }
        }
    }
}
#endif

static char *
plan_render_itinerary (struct itinerary *itin, tdata_t *tdata, time_t date,
                       char *b, char *b_end) {
    leg_t *leg;

    b += sprintf (b, "\nITIN %d rides \n", itin->n_rides);

    /* Render the legs of this itinerary, which are in chronological order */
    for (leg = itin->legs; leg < itin->legs + itin->n_legs; ++leg) {
        char ct0[16];
        char ct1[16];
        char *alert_msg = NULL;
        const char *operator_name, *short_name, *headsign, *commercial_mode;
        const char *leg_mode = NULL;
        const char *s0_id = tdata_stop_point_name_for_index(tdata, leg->sp_from);
        const char *s1_id = tdata_stop_point_name_for_index(tdata, leg->sp_to);
        float d0 = 0.0, d1 = 0.0;

        btimetext(leg->t0, ct0);
        btimetext(leg->t1, ct1);

        if (leg->journey_pattern == WALK) {
            operator_name = "";
            short_name = "walk";
            headsign = "walk";
            commercial_mode = "";

            /* Skip uninformative legs that just tell you to stay in the same
             * place. if (leg->s0 == leg->s1) continue;
             */
            if (leg->sp_from == ONBOARD) continue;
            if (leg->sp_from == leg->sp_to) leg_mode = "WAIT";
            else leg_mode = "WALK";
        } else {
            operator_name = tdata_operator_name_for_journey_pattern(tdata, leg->journey_pattern);
            short_name = tdata_line_code_for_journey_pattern(tdata, leg->journey_pattern);
            commercial_mode = tdata_commercial_mode_name_for_journey_pattern(tdata, leg->journey_pattern);
            #ifdef RRRR_FEATURE_REALTIME_EXPANDED
            headsign = tdata_headsign_for_journey_pattern_point(tdata, leg->journey_pattern,leg->jpp0);
            d0 = leg->d0 / 60.0f;
            d1 = leg->d1 / 60.0f;
            #else
            headsign = tdata_headsign_for_journey_pattern(tdata, leg->journey_pattern);
            #endif

            leg_mode = tdata_physical_mode_name_for_journey_pattern(tdata, leg->journey_pattern);

            #ifdef RRRR_FEATURE_REALTIME_ALERTS
            if (leg->journey_pattern != WALK && tdata->alerts) {
                leg_add_alerts (leg, tdata, date, &alert_msg);
            }
            #else
            UNUSED(date);
            #endif
        }

        /* TODO: we are able to calculate the maximum length required for each line
         * therefore we could prevent a buffer overflow from happening. */
        b += sprintf (b, "%s %5d %3d %5d %5d %s %+3.1f %s %+3.1f ;%s;%s;%s;%s;%s;%s;%s\n",
            leg_mode, leg->journey_pattern, leg->vj, leg->sp_from, leg->sp_to, ct0, d0, ct1, d1, operator_name, short_name, headsign, commercial_mode, s0_id, s1_id,
                        (alert_msg ? alert_msg : ""));

        /* EXAMPLE
        polyline_for_leg (tdata, leg);
        b += sprintf (b, "%s\n", polyline_result());
        */

        if (b > b_end) {
            fprintf (stderr, "buffer overflow\n");
            return b;
            /* exit(2); */
        }
    }

    return b;
}

/* Write a plan structure out to a text buffer in tabular format. */
static uint32_t
plan_render(plan_t *plan, tdata_t *tdata, router_request_t *req,
            char *buf, uint32_t buflen) {
    char *b = buf;
    char *b_end = buf + buflen;
    time_t date = router_request_to_date (req, tdata, NULL);

    if ((req->optimise & o_all) == o_all) {
        /* Iterate over itineraries in this plan,
         * which are in increasing order of number of rides
         */
        itinerary_t *itin;
        for (itin = plan->itineraries;
             itin < plan->itineraries + plan->n_itineraries;
             ++itin) {
            b = plan_render_itinerary (itin, tdata, date, b, b_end);
        }
    } else if (plan->n_itineraries > 0) {
        /* only render the first itinerary,
         * which has the least transfers
         */
        if ((req->optimise & o_transfers) == o_transfers) {
           b = plan_render_itinerary (plan->itineraries, tdata, date, b, b_end);
        }

        /* only render the last itinerary,
         * which has the most rides and is the shortest in time
         */
        if ((req->optimise & o_shortest) == o_shortest) {
            b = plan_render_itinerary (&plan->itineraries[plan->n_itineraries - 1], tdata, date, b, b_end);
        }
    }
    *b = '\0';
    return b - buf;
}

/* After routing, call to convert the router state into a readable list of
 * itinerary legs. Returns the number of bytes written to the buffer.
 */
uint32_t
router_result_dump(router_t *router, router_request_t *req,
                   char *buf, uint32_t buflen) {
    plan_t plan;
    if (!router_result_to_plan (&plan, router, req)) {
        return 0;
    }

    /* plan_render_json (&plan, router->tdata, req); */
    return plan_render (&plan, router->tdata, req, buf, buflen);
}

