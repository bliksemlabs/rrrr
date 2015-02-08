/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* router.c : the main routing algorithm */
#include "router.h" /* first to ensure it works alone */
#include "router_request.h"
#include "router_dump.h"

#include "util.h"
#include "config.h"
#include "tdata.h"
#include "bitset.h"
#include "hashgrid.h"
#include "set.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <sys/types.h>

bool router_setup(router_t *router, tdata_t *tdata) {
    uint64_t n_states = tdata->n_stop_points * RRRR_DEFAULT_MAX_ROUNDS;
    router->tdata = tdata;
    router->best_time = (rtime_t *) malloc(sizeof(rtime_t) * tdata->n_stop_points);
    router->states_back_journey_pattern = (jpidx_t *) malloc(sizeof(jpidx_t) * n_states);
    router->states_back_vehicle_journey = (jp_vjoffset_t *) malloc(sizeof(jp_vjoffset_t) * n_states);
    router->states_ride_from = (spidx_t *) malloc(sizeof(spidx_t) * n_states);
    router->states_walk_from = (spidx_t *) malloc(sizeof(spidx_t) * n_states);
    router->states_walk_time = (rtime_t *) malloc(sizeof(rtime_t) * n_states);
    router->states_time = (rtime_t *) malloc(sizeof(rtime_t) * n_states);
    router->states_board_time = (rtime_t *) malloc(sizeof(rtime_t) * n_states);

    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    router->states_back_journey_pattern_point = (jppidx_t *) malloc(sizeof(jppidx_t) * n_states);
    router->states_journey_pattern_point = (jppidx_t *) malloc(sizeof(jppidx_t) * n_states);
    #endif

    router->updated_stop_points = bitset_new(tdata->n_stop_points);
    router->updated_walk_stop_points = bitset_new(tdata->n_stop_points);
    router->updated_journey_patterns = bitset_new(tdata->n_journey_patterns);

#if RRRR_BANNED_JOURNEY_PATTERNS_BITMASK == 1
    router->banned_journey_patterns = bitset_new(tdata->n_journey_patterns);
#endif

    if ( ! (router->best_time
            && router->states_back_journey_pattern
            && router->states_back_vehicle_journey
            && router->states_ride_from
            && router->states_walk_from
            && router->states_walk_time
            && router->states_time
            && router->states_board_time
#ifdef RRRR_FEATURE_REALTIME_EXPANDED
            && router->states_back_journey_pattern_point
            && router->states_journey_pattern_point
#endif
            && router->updated_stop_points
            && router->updated_walk_stop_points
            && router->updated_journey_patterns
#if RRRR_BANNED_JOURNEY_PATTERNS_BITMASK == 1
            && router->banned_journey_patterns
#endif
           )
       ) {
        fprintf(stderr, "failed to allocate router scratch space");
        return false;
    }

    return true;
}

void router_teardown(router_t *router) {
    free(router->best_time);
    free(router->states_back_journey_pattern);
    free(router->states_back_vehicle_journey);
    free(router->states_ride_from);
    free(router->states_walk_from);
    free(router->states_walk_time);
    free(router->states_time);
    free(router->states_board_time);
#ifdef RRRR_FEATURE_REALTIME_EXPANDED
    free(router->states_back_journey_pattern_point);
    free(router->states_journey_pattern_point);
#endif
    bitset_destroy(router->updated_stop_points);
    bitset_destroy(router->updated_walk_stop_points);
    bitset_destroy(router->updated_journey_patterns);

#if RRRR_BANNED_JOURNEY_PATTERNS_BITMASK == 1
    bitset_destroy(router->banned_journey_patterns);
#endif
}

void router_reset(router_t *router) {

    /* Make sure both origin and target are initialised with NONE, so it
     * becomes possible to validate that they have been set to a valid
     * stop_point index.
     */
    router->origin = STOP_NONE;
    router->target = STOP_NONE;

    /* The best times to arrive at a stop_point scratch space is initialised with
     * UNREACHED. This allows to compare for a lesser time candidate in the
     * search.
     */
    rrrr_memset (router->best_time, UNREACHED, router->tdata->n_stop_points);
    rrrr_memset (router->states_board_time, UNREACHED, router->tdata->n_stop_points);
}

static bool initialize_states (router_t *router) {
    uint64_t i_state = ((uint64_t) RRRR_DEFAULT_MAX_ROUNDS) * router->tdata->n_stop_points;

    do {
        i_state--;
        router->states_time[i_state] = UNREACHED;
        router->states_walk_time[i_state] = UNREACHED;
    } while (i_state);

    return true;
}


/* One serviceday_t for each of: yesterday, today, tomorrow (for overnight
 * searches). Note that yesterday's bit flag will be 0 if today is the
 * first day of the calendar.
 */
static bool initialize_servicedays (router_t *router, router_request_t *req) {
    /* One bit for the calendar day on which realtime data should be
     * applied (applying only on the true current calendar day)
     */
    serviceday_t yesterday, today, tomorrow;
    uint8_t day_i = 0;
    calendar_t realtime_mask;
    router->day_mask = req->day_mask;

    /*  Fake realtime_mask if we are QA testing. */
    #ifdef RRRR_FAKE_REALTIME
    realtime_mask = ~((calendar_t) 0);
    #else
    realtime_mask = ((calendar_t) 1) << ((time(NULL) - router->tdata->calendar_start_time) /
                                         SEC_IN_ONE_DAY);
    #endif

    yesterday.midnight = 0;
    yesterday.mask = router->day_mask >> 1;
    yesterday.apply_realtime = (bool) (yesterday.mask & realtime_mask);
    today.midnight = RTIME_ONE_DAY;
    today.mask = router->day_mask;
    today.apply_realtime = (bool) (today.mask & realtime_mask);
    tomorrow.midnight = RTIME_TWO_DAYS;
    tomorrow.mask = router->day_mask << 1;
    tomorrow.apply_realtime = (bool) (tomorrow.mask & realtime_mask);

    router->day_mask = today.mask;
    /* Iterate backward over days for arrive-by searches. */
    if (req->arrive_by) {
        if (req->time > tomorrow.midnight) {
            /* Departuretime includes trips running tomorrow */
            router->servicedays[day_i] = tomorrow;
            router->day_mask |= tomorrow.mask;
            day_i++;
        }
        if (req->time_cutoff < (today.midnight + router->tdata->max_time) && req->time > today.midnight) {
            router->servicedays[day_i] = today;
            router->day_mask |= today.mask;
            day_i++;
        }
        if (req->time_cutoff < router->tdata->max_time){
            /* Cut-off request includes vehicle_journey running tomorrow */
            router->servicedays[day_i] = yesterday;
            router->day_mask |= yesterday.mask;
            day_i++;
        }
    } else {
        if (req->time < router->tdata->max_time){
            /* There are still some journeys running of previous day on departure_time of journey */
            router->day_mask |= yesterday.mask;
            router->servicedays[day_i] = yesterday;
            day_i++;
        }
        if (req->time_cutoff >= today.midnight && req->time < (today.midnight + router->tdata->max_time)) {
            router->servicedays[day_i] = today;
            router->day_mask |= today.mask;
            day_i++;
        }
        if (req->time_cutoff > tomorrow.midnight){
            /* Cut-off request includes vehicle_journey running tomorrow */
            router->day_mask |= tomorrow.mask;
            router->servicedays[day_i] = tomorrow;
            day_i++;
        }
    }
    router->n_servicedays = day_i;

    #ifdef RRRR_DEBUG
    {
        uint8_t i;
        for (i = 0; i < 3; ++i) service_day_dump (&router->servicedays[i]);
        day_mask_dump (router->day_mask);
    }
    #endif

    return true;
}

/* Given a stop_point index, mark all journey_patterns that serve it as updated. */
static void flag_journey_patterns_for_stop_point(router_t *router, router_request_t *req,
        spidx_t sp_index) {
    jpidx_t *journey_patterns;
    uint32_t i_jp = tdata_journey_patterns_for_stop_point(router->tdata, sp_index,
            &journey_patterns);

    if (i_jp == 0) return;

    do {
        calendar_t jp_active_flags;

        i_jp--;

        #ifdef RRRR_INFO
        fprintf (stderr, "flagging journey_pattern %d at stop_point %d\n",
                         journey_patterns[i_jp], sp_index);
        #endif

        jp_active_flags = router->tdata->journey_pattern_active[journey_patterns[i_jp]];

        /* CHECK that there are any vehicle_journeys running on this journey_pattern
         * (another bitfield)
         * fprintf(stderr, "journey_pattern flags %d", jp_active_flags);
         * printBits(4, &jp_active_flags);
         */

        /* & journey_pattern_active_flags seems to provide about 14% increase
         * in throughput
         */
        if ((router->day_mask & jp_active_flags) &&
            (req->mode & router->tdata->journey_patterns[journey_patterns[i_jp]].attributes) > 0) {
           bitset_set (router->updated_journey_patterns, journey_patterns[i_jp]);
           #ifdef RRRR_INFO
           fprintf (stderr, "  journey_pattern running\n");
           #endif
        }
    } while (i_jp);

    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    if (router->servicedays[1].apply_realtime &&
        router->tdata->rt_journey_patterns_at_stop_point[sp_index]) {
        journey_patterns = router->tdata->rt_journey_patterns_at_stop_point[sp_index]->list;
        i_jp = router->tdata->rt_journey_patterns_at_stop_point[sp_index]->len;
        if (i_jp == 0) return;
        do {
            i_jp--;

            #ifdef RRRR_INFO
            fprintf (stderr, "  flagging changed journey_pattern %d at stop_point %d\n",
                             journey_patterns[i_jp], sp_index);
            #endif
            /* extra journey_patterns should only be applied on the current day */
            if ((req->mode & router->tdata->journey_patterns[journey_patterns[i_jp]].attributes) > 0) {
                bitset_set (router->updated_journey_patterns, journey_patterns[i_jp]);
                #ifdef RRRR_INFO
                fprintf (stderr, "  journey_pattern running\n");
                #endif
            }
        } while (i_jp);
    }
    #endif
}

#if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
#if RRRR_BANNED_JOURNEY_PATTERNS_BITMASK == 1
static void unflag_banned_journey_patterns (router_t *router, router_request_t *req) {
    if (req->n_banned_journey_patterns == 0) return;

    bitset_mask_and (router->updated_journey_patterns, router->banned_journey_patterns);
}

static void initialize_banned_journey_patterns (router_t *router, router_request_t *req) {
    uint8_t i_banned_jp = req->n_banned_journey_patterns;

    bitset_black(router->banned_journey_patterns);

    if (i_banned_jp == 0) return;
    do {
        i_banned_jp--;
         bitset_unset (router->banned_journey_patterns,
                       req->banned_journey_patterns[i_banned_jp]);
    } while (i_banned_jp);
}
#else
static void unflag_banned_journey_patterns(router_t *router, router_request_t *req) {
    uint8_t i_banned_jp = req->n_banned_journey_patterns;
    if (i_banned_jp == 0) return;
    do {
        i_banned_jp--;
        bitset_unset (router->updated_journey_patterns,
                      req->banned_journey_patterns[i_banned_jp]);
    } while (i_banned_jp);
}
#endif
#endif

#if RRRR_MAX_BANNED_STOP_POINTS > 0
static void unflag_banned_stop_points(router_t *router, router_request_t *req) {
    uint8_t i_banned_sp = req->n_banned_stops;
    if (i_banned_sp == 0) return;
    do {
        i_banned_sp--;
        bitset_unset (router->updated_stop_points,
                      req->banned_stops[i_banned_sp]);
    } while (i_banned_sp);
}
#endif

#ifdef RRRR_FEATURE_LATLON
/* Because the first round begins with so few reached stops, the initial state
 * doesn't get its own full array of states
 * Instead we reuse one of the later rounds (round 1) for the initial state.
 * This means we need to reset the walks in round 1 back to UNREACHED before
 * using them in routing. We iterate over all of them, because we don't
 * maintain a list of all the stops that might have been added by the hashgrid.
 */
static void initialize_transfers_full (router_t *router, uint32_t round) {
    uint32_t i_state = router->tdata->n_stop_points;
    rtime_t *states_walk_time = router->states_walk_time + (round * router->tdata->n_stop_points);
    do {
        i_state--;
        states_walk_time[i_state] = UNREACHED;
    } while (i_state);
}
#else

/* Because the first round begins with so few reached stops, the initial state
 * doesn't get its own full array of states. Instead we reuse one of the later
 * rounds (round 1) for the initial state. This means we need to reset the
 * walks in round 1 back to UNREACHED before using them in routing. Rather
 * than iterating over all of them, we only initialize the stops that can be
 * reached by transfers.
 * Alternatively we could instead initialize walks to UNREACHED at the
 * beginning of the transfer calculation function.
 * We should not however reset the best times for those stops reached from the
 * initial stop_point on foot. This will prevent finding circuitous itineraries that
 * return to them.
 */
static void initialize_transfers (router_t *router,
                                  uint32_t round, spidx_t sp_index_from) {
    rtime_t *states_walk_time = router->states_walk_time + (round * router->tdata->n_stop_points);
    uint32_t t  = router->tdata->stop_points[sp_index_from    ].transfers_offset;
    uint32_t tN = router->tdata->stop_points[sp_index_from + 1].transfers_offset;
    states_walk_time[sp_index_from] = UNREACHED;
    for ( ; t < tN ; ++t) {
        spidx_t sp_index_to = router->tdata->transfer_target_stops[t];
        states_walk_time[sp_index_to] = UNREACHED;
    }
}
#endif


/* Get the departure or arrival time of the given vj on the given
 * service day, applying realtime data as needed.
 */
static rtime_t
tdata_stoptime (tdata_t* tdata, serviceday_t *serviceday,
                jpidx_t jp_index, jp_vjoffset_t vj_offset, jppidx_t journey_pattern_point,
                bool arrive) {
    rtime_t time, time_adjusted;
    stoptime_t *vj_stoptimes;

    /* This code is only required if want realtime support in
     * the journey planner
     */
    #ifdef RRRR_FEATURE_REALTIME_EXPANDED

    /* given that we are at a serviceday the realtime scope applies to */
    if (serviceday->apply_realtime) {

        /* the expanded stoptimes can be found at the same row as the vehicle_journey */
        vj_stoptimes = tdata->vj_stoptimes[tdata->journey_patterns[jp_index].vj_offset + vj_offset];

        if (vj_stoptimes) {
            /* if the expanded stoptimes have been added,
             * the begin_time is precalculated
             */
            time = 0;
        } else {
            /* if the expanded stoptimes have not been added,
             * or our source is not time-expanded
             */
            vehicle_journey_t *vj = tdata_vehicle_journeys_in_journey_pattern(tdata, jp_index) + vj_offset;
            vj_stoptimes = &tdata->stop_times[vj->stop_times_offset];
            time = vj->begin_time;
        }
    } else
    #endif /* RRRR_FEATURE_REALTIME_EXPANDED */
    {
        vehicle_journey_t *vj = tdata_vehicle_journeys_in_journey_pattern(tdata, jp_index) + vj_offset;
        vj_stoptimes = &tdata->stop_times[vj->stop_times_offset];
        time = vj->begin_time;
    }

    if (arrive) {
        time += vj_stoptimes[journey_pattern_point].arrival;
    } else {
        time += vj_stoptimes[journey_pattern_point].departure;
    }

    time_adjusted = time + serviceday->midnight;

    /*
    printf ("boarding at stop_point %d, time is: %s \n", journey_pattern_point, timetext (time));
    printf ("   after adjusting: %s \n", timetext (time_adjusted));
    printf ("   midnight: %d \n", serviceday->midnight);
    */

    /* Detect overflow (this will still not catch wrapping due to negative
     * delays on small positive times) actually this happens naturally with
     * times like '03:00+1day' transposed to serviceday 'tomorrow'
     */
    if (time_adjusted < time) return UNREACHED;
    return time_adjusted;
}

/* TODO: change the function name of tdata_next */
static bool
tdata_next (router_t *router, router_request_t *req,
            jpidx_t jp_index, jp_vjoffset_t vj_offset, rtime_t qtime,
            spidx_t *ret_sp_index, rtime_t *ret_stop_time) {

    spidx_t *journey_pattern_points = tdata_points_for_journey_pattern(router->tdata, jp_index);
    journey_pattern_t *jp = router->tdata->journey_patterns + jp_index;
    uint32_t jpp_i;

    *ret_sp_index = STOP_NONE;
    *ret_stop_time = UNREACHED;

    for (jpp_i = 0; jpp_i < jp->n_stops; ++jpp_i) {
        /* TODO: check if the arrive = false flag works with req->arrive_by */

        rtime_t time = tdata_stoptime (router->tdata, &(router->servicedays[1]),
                jp_index, vj_offset, (jppidx_t) jpp_i, false);

        /* Find stop_point immediately after the given time on the given vj. */
        if (req->arrive_by ? time > qtime : time < qtime) {
            if (*ret_stop_time == UNREACHED ||
                (req->arrive_by ? time < *ret_stop_time :
                                  time > *ret_stop_time)) {
                *ret_sp_index = (spidx_t) journey_pattern_points[jpp_i];
                *ret_stop_time = time;
            }
        }
    }

    return (*ret_sp_index != STOP_NONE);
}

/* For each updated stop_point and each destination of a transfer from an updated
 * stop_point, set the associated journey_patterns as updated. The journey_patterns bitset is cleared
 * before the operation, and the stop_points bitset is cleared after all transfers
 * have been computed and all journey_patterns have been set.
 * Transfer results are computed within the same round, based on arrival time
 * in the ride phase and stored in the walk time member of states.
 */
static void apply_transfers (router_t *router, router_request_t *req,
                      uint32_t round, bool transfer, bool initial) {
    rtime_t *states_time = router->states_time + (round * router->tdata->n_stop_points);
    rtime_t *states_walk_time = router->states_walk_time + (round * router->tdata->n_stop_points);
    spidx_t *states_walk_from = router->states_walk_from + (round * router->tdata->n_stop_points);
    uint32_t sp_index_from; /* uint32_t: because we need to compare to BITSET_NONE */

    /* The transfer process will flag journey_patterns that should be explored in
     * the next round.
     */
    bitset_clear (router->updated_journey_patterns);
    for (sp_index_from = bitset_next_set_bit (router->updated_stop_points, 0);
         sp_index_from != BITSET_NONE;
         sp_index_from = bitset_next_set_bit (router->updated_stop_points, sp_index_from + 1)) {
        rtime_t time_from = states_time[sp_index_from];
        #ifdef RRRR_INFO
        printf ("stop_point %d was marked as updated \n", sp_index_from);
        #endif

        if (time_from == UNREACHED) {
            fprintf (stderr, "ERROR: transferring from unreached stop_point %d in round %d. \n", sp_index_from, round);
            continue;
        }
        /* At this point, the best time at the from stop_point may be different than
         * the state_from->time, because the best time may have been updated
         * by a transfer.
         */
        #ifdef RRRR_DEBUG
        if (time_from != router->best_time[sp_index_from]) {
            char buf[13];

            fprintf (stderr, "ERROR: time at stop_point %d in round %d " \
                             "is not the same as its best time. \n",
                    sp_index_from, round);
            fprintf (stderr, "    from time %s \n",
                     btimetext(time_from, buf));
            fprintf (stderr, "    walk time %s \n",
                     btimetext(states_walk_time[sp_index_from], buf));
            fprintf (stderr, "    best time %s \n",
                     btimetext(router->best_time[sp_index_from], buf));
            continue;
        }
        #endif
        #ifdef RRRR_INFO
        fprintf (stderr, "  applying transfer at %d (%s) \n", sp_index_from,
                 tdata_stop_point_name_for_index(router->tdata, sp_index_from));
        #endif

        if (states_time[sp_index_from] == router->best_time[sp_index_from]) {
            rtime_t sp_waittime = tdata_stop_point_waittime(router->tdata, (spidx_t) sp_index_from);
            /* This state's best time is still its own.
             * No improvements from other transfers.
             */
            if (initial){
                states_walk_time[sp_index_from] = time_from;
            }else if (req->arrive_by){
                states_walk_time[sp_index_from] = time_from - sp_waittime;
            }else{
                states_walk_time[sp_index_from] = time_from + sp_waittime;
            }
            states_walk_from[sp_index_from] = (spidx_t) sp_index_from;
            /* assert (router->best_time[stop_index_from] == time_from); */
            bitset_set(router->updated_walk_stop_points, sp_index_from);
        }


        if (transfer) {
        /* Then apply transfers from the stop_point to nearby stops */
        uint32_t tr     = router->tdata->stop_points[sp_index_from].transfers_offset;
        uint32_t tr_end = router->tdata->stop_points[sp_index_from + 1].transfers_offset;
        for ( ; tr < tr_end ; ++tr) {
            /* Transfer durations are stored in r_time */
            spidx_t sp_index_to = router->tdata->transfer_target_stops[tr];
            rtime_t transfer_duration = router->tdata->transfer_durations[tr] + req->walk_slack;
            rtime_t time_to = req->arrive_by ? time_from - transfer_duration
                                             : time_from + transfer_duration;

            /* Avoid reserved values including UNREACHED
             * and catch wrapping/overflow due to limited range of rtime_t
             * this happens normally on overnight routing but should
             * be avoided rather than caught.
             */
            if (time_to > RTIME_THREE_DAYS || (req->arrive_by ? time_to > time_from :time_to < time_from)) continue;

            #ifdef RRRR_INFO
            {
                char buf[13];
                fprintf (stderr, "    target %d %s (%s) \n",
                  sp_index_to,
                  btimetext(router->best_time[sp_index_to], buf),
                  tdata_stop_point_name_for_index(router->tdata, sp_index_to));

                fprintf (stderr, "    transfer time   %s\n",
                        btimetext(transfer_duration, buf));

                fprintf (stderr, "    transfer result %s\n",
                        btimetext(time_to, buf));
            }
            #endif

            /* TODO verify state_to->walk_time versus
             *             router->best_time[sp_index_to] */
            if (router->best_time[sp_index_to] == UNREACHED ||
                (req->arrive_by ? time_to > router->best_time[sp_index_to]
                                : time_to < router->best_time[sp_index_to])) {

                #ifdef RRRR_INFO
                char buf[13];
                fprintf (stderr, "      setting %d to %s\n",
                         sp_index_to, btimetext(time_to, buf));
                #endif
                states_walk_time[sp_index_to] = time_to;
                states_walk_from[sp_index_to] = (spidx_t) sp_index_from;
                router->best_time[sp_index_to] = time_to;
                bitset_set(router->updated_walk_stop_points, sp_index_to);
            }
        }
        }
    }

    for (sp_index_from = bitset_next_set_bit (router->updated_walk_stop_points, 0);
         sp_index_from != BITSET_NONE;
         sp_index_from = bitset_next_set_bit (router->updated_walk_stop_points, sp_index_from + 1)) {
        flag_journey_patterns_for_stop_point(router, req, (spidx_t) sp_index_from);
    }

    #if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
    unflag_banned_journey_patterns(router, req);
    #endif

    /* Done with all transfers, reset stop-reached bits for the next round */
    bitset_clear (router->updated_stop_points);
    bitset_clear (router->updated_walk_stop_points);
    /* Check invariant: Every stop_point reached in this round should have a
     * best time equal to its walk time, and
     * a walk arrival time <= its ride arrival time.
     */
}

/**
* Search a better time for the best time to board when no journey_pattern before has been boarded along this journey_pattern
* Start with the best possibility (arrive_by: LAST departure, depart_after FIRST departure) and end with the worst possibility,
* unless we reach a vehicle_journey that matches our requirements (valid departure, valid calendar, valid trip_attributes)
*/
static void board_vehicle_journeys_within_days(router_t *router, router_request_t *req,
        jpidx_t jp_index,
        jppidx_t jpp_offset,
        rtime_t prev_time,
        serviceday_t **best_serviceday,
        jp_vjoffset_t *best_vj, rtime_t *best_time) {

    calendar_t *vj_masks = tdata_vj_masks_for_journey_pattern(router->tdata, jp_index);
    vehicle_journey_t *vjs_in_journey_pattern = tdata_vehicle_journeys_in_journey_pattern(router->tdata, jp_index);
    journey_pattern_t *jp = &(router->tdata->journey_patterns[jp_index]);
    serviceday_t *serviceday;
    bool jp_overlap = jp->min_time < (jp->max_time - RTIME_ONE_DAY);

    /* Search through the servicedays that are assumed to be put in search-order (counterclockwise for arrive_by)
     */
    for (serviceday  = router->servicedays;
         serviceday <= router->servicedays + router->n_servicedays;
         ++serviceday) {

        int32_t i_vj_offset;

        /* Check that this journey_pattern still has any vehicle_journeys
         * running on this day.
         */
        if (req->arrive_by ? prev_time < serviceday->midnight +
                                         jp->min_time
                           : prev_time > serviceday->midnight +
                                         jp->max_time) continue;

        /* Check whether there's any chance of improvement by
         * scanning additional days. Note that day list is
         * reversed for arrive-by searches.
         */
        if (*best_vj != NONE && !jp_overlap) break;

        for (i_vj_offset = req->arrive_by ? jp->n_vjs - 1: 0;
             req->arrive_by ? i_vj_offset >= 0
                            : i_vj_offset < jp->n_vjs;
             req->arrive_by ? --i_vj_offset
                            : ++i_vj_offset) {
            rtime_t time;

            #ifdef RRRR_DEBUG
            printBits(4, & (vj_masks[i_vj_offset]));
            printBits(4, & (serviceday->mask));
            fprintf(stderr, "\n");
            #endif

            #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
            /* skip this vj if it is banned */
            if (set_in_vj (req->banned_vjs_journey_pattern, req->banned_vjs_offset,
                           req->n_banned_vjs, jp_index,
                           (jp_vjoffset_t) i_vj_offset)) continue;
            #endif

            /* skip this vj if it is not running on
             * the current service day
             */
            if ( ! (serviceday->mask & vj_masks[i_vj_offset])) continue;
            /* skip this vj if it doesn't have all our
             * required attributes
             * Checking whether we have required req->vj_attributes at all, before checking the attributes of the vehicle_journeys
             * is about 4% more efficient for journeys without specific vj attribute requirements.
             */
            if (req->vj_attributes && ! ((req->vj_attributes & vjs_in_journey_pattern[i_vj_offset].vj_attributes) == req->vj_attributes)) continue;

            /* consider the arrival or departure time on
             * the current service day
             */
            time = tdata_stoptime (router->tdata, serviceday, jp_index, (jp_vjoffset_t) i_vj_offset, jpp_offset, req->arrive_by);

            #ifdef RRRR_DEBUG_VEHICLE_JOURNEY
            fprintf(stderr, "    board option %d at %s \n", i_vj_offset, "");
            #endif

            /* rtime overflow due to long overnight vehicle_journey's on day 2 */
            if (time == UNREACHED) continue;

            if (req->arrive_by ? time < req->time_cutoff
                               : time > req->time_cutoff){
                return;
            }

            /* Mark vj for boarding if it improves on the last round's
             * post-walk time at this stop. Note: we should /not/ be comparing
             * to the current best known time at this stop, because it may have
             * been updated in this round by another vj (in the pre-walk
             * transit phase).
             */
            if (req->arrive_by ? time <= prev_time && time > *best_time
                               : time >= prev_time && time < *best_time) {
                *best_vj = (jp_vjoffset_t) i_vj_offset;
                *best_time = time;
                *best_serviceday = serviceday;
                /* Since FIFO ordering of trips is ensured, we can immediately return if the JourneyPattern does not overlap.
                 * If the JourneyPattern does overlap, we can only return if the time does not fall in the overlapping period
                 */
                if (!jp_overlap || jp->min_time > time - RTIME_ONE_DAY)
                    return;
            }
        }  /*  end for (vehicle_journey's within this route) */
    }  /*  end for (service days: yesterday, today, tomorrow) */
}

/**
* Search a better time for the best time to board when we already boarded a vehicle_journey on this journey_pattern.
* Assume the currently boarded VJ is the best and try to improve that time,
* since there usually non to very few better vehicle_journeys, this reduces the workload significantly
* We are scanning counterclockwise for arrive_by and clockwise for depart_after.
*/
static void reboard_vehicle_journeys_within_days(router_t *router, router_request_t *req,
        jpidx_t jp_index,
        jppidx_t jpp_offset,
        serviceday_t *prev_serviceday,
        jp_vjoffset_t prev_vj_offset,
        rtime_t prev_time,
        serviceday_t **best_serviceday,
        jp_vjoffset_t *best_vj, rtime_t *best_time) {

    calendar_t *vj_masks = tdata_vj_masks_for_journey_pattern(router->tdata, jp_index);
    vehicle_journey_t *vjs_in_journey_pattern = tdata_vehicle_journeys_in_journey_pattern(router->tdata, jp_index);
    journey_pattern_t *jp = &(router->tdata->journey_patterns[jp_index]);

    serviceday_t *serviceday;

    /* Search service_days in reverse order. (arrive_by low to high, else high to low) */
    for (serviceday = prev_serviceday;
         serviceday >= router->servicedays;
         --serviceday) {
        int32_t i_vj_offset;

        /* Check that this journey_pattern still has any suitable vehicle_journeys
         * running on this day.
         */
        if (req->arrive_by ? prev_time < serviceday->midnight + jp->min_time
                           : prev_time > serviceday->midnight + jp->max_time) continue;

        for (i_vj_offset = prev_vj_offset;
             req->arrive_by ? i_vj_offset < jp->n_vjs :
                              i_vj_offset >= 0;
             req->arrive_by ? ++i_vj_offset
                            : --i_vj_offset) {
            rtime_t time;

            #ifdef RRRR_DEBUG
            printBits(4, & (vj_masks[i_vj_offset]));
            printBits(4, & (serviceday->mask));
            fprintf(stderr, "\n");
            #endif

            #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
            /* skip this vj if it is banned */
            if (set_in_vj (req->banned_vjs_journey_pattern, req->banned_vjs_offset,
                           req->n_banned_vjs, jp_index,
                           (jp_vjoffset_t) i_vj_offset)) continue;
            #endif

            /* skip this vj if it is not running on
             * the current service day
             */
            if ( ! (serviceday->mask & vj_masks[i_vj_offset])) continue;
            /* skip this vj if it doesn't have all our
             * required attributes
             * Checking whether we have required req->vj_attributes at all, before checking the attributes of the vehicle_journeys
             * is about 4% more efficient for journeys without specific vj attribute requirements.
             */
            if (req->vj_attributes && ! ((req->vj_attributes & vjs_in_journey_pattern[i_vj_offset].vj_attributes) == req->vj_attributes)) continue;

            /* consider the arrival or departure time on
             * the current service day
             */
            time = tdata_stoptime (router->tdata, serviceday, jp_index, i_vj_offset, jpp_offset, req->arrive_by);

            #ifdef RRRR_DEBUG_VEHICLE_JOURNEY
            fprintf(stderr, "    board option %d at %s \n", i_vj_offset, "");
            #endif

            /* rtime overflow due to long overnight vehicle_journey's on day 2 */
            if (time == UNREACHED) continue;

            if (req->arrive_by ? time > prev_time
                               : time < prev_time){
                return;
            }

            /* Mark vj for boarding if it improves on the last round's
             * post-walk time at this stop. Note: we should /not/ be comparing
             * to the current best known time at this stop, because it may have
             * been updated in this round by another vj (in the pre-walk
             * transit phase).
             */
            if (req->arrive_by ? time <= prev_time && time > *best_time
                    : time >= prev_time && time < *best_time) {
                *best_vj = i_vj_offset;
                *best_time = time;
                *best_serviceday = serviceday;
            }
        }  /*  end for (vehicle_journey's within this route) */

        /* Reset the VJ offset to the highest value, after we scanned the original serviceday */
        prev_vj_offset = req->arrive_by ? 0 : jp->n_vjs - 1;

    }  /*  end for (service days: yesterday, today, tomorrow) */
}

static bool
write_state(router_t *router, router_request_t *req,
            uint8_t round, jpidx_t jp_index, jp_vjoffset_t vj_offset,
            spidx_t sp_index, jppidx_t jpp_offset, rtime_t time,
            spidx_t board_stop, jppidx_t board_jpp_offset,
            rtime_t board_time) {

    uint64_t i_state = ((uint64_t) round) * router->tdata->n_stop_points + sp_index;

    #ifndef RRRR_REALTIME
    UNUSED (jpp_offset);
    UNUSED (board_jpp_offset);
    #endif

    #ifdef RRRR_INFO
    {
        char buf[13];
        fprintf(stderr, "    setting stop_point to %s \n", btimetext(time, buf));
    }
    #endif

    router->best_time[sp_index]    = time;
    router->states_time[i_state]                 = time;
    router->states_back_journey_pattern[i_state] = jp_index;
    router->states_back_vehicle_journey[i_state] = vj_offset;
    router->states_ride_from[i_state]            = board_stop;
    router->states_board_time[i_state]           = board_time;
    #ifdef RRRR_FEATURE_REALTIME_EXPANDED
    router->states_back_journey_pattern_point[i_state] = board_jpp_offset;
    router->states_journey_pattern_point[i_state]      = jpp_offset;
    #endif

    #ifdef RRRR_STRICT
    if (req->arrive_by && board_time < time) {
        fprintf (stderr, "board time non-decreasing\n");
        return false;
    } else if (!req->arrive_by && board_time > time) {
        fprintf (stderr, "board time non-increasing\n");
        return false;
    }
    #else
    UNUSED (req);
    #endif

    return true;
}

static void router_round(router_t *router, router_request_t *req, uint8_t round) {
    /*  TODO restrict pointers? */
    rtime_t *states_walk_time = router->states_walk_time + (((round == 0) ? 1 : round - 1) * router->tdata->n_stop_points);
    uint32_t jp_index;

    #ifdef RRRR_INFO
    fprintf(stderr, "round %d\n", round);
    #endif
    /* Iterate over all journey_patterns which contain a stop_point that was updated in the last round. */
    for (jp_index = bitset_next_set_bit (router->updated_journey_patterns, 0);
         jp_index != BITSET_NONE;
         jp_index = bitset_next_set_bit (router->updated_journey_patterns, jp_index + 1)) {

        journey_pattern_t *jp = &(router->tdata->journey_patterns[jp_index]);
        spidx_t *journey_pattern_points = tdata_points_for_journey_pattern(router->tdata, (jpidx_t) jp_index);
        uint8_t *journey_pattern_point_attributes = tdata_stop_point_attributes_for_journey_pattern(router->tdata, jp_index);

        /* Service day on which that vj was boarded */
        serviceday_t *board_serviceday = NULL;

        /* vj index within the journey_pattern. NONE means not yet boarded. */
        jp_vjoffset_t vj_index = NONE;

        /* stop_point index where that vj was boarded */
        spidx_t       board_sp  = 0;

        /* journey_pattern_point index where that vj was boarded */
        jppidx_t      board_jpp = 0;

        /* time when that vj was boarded */
        rtime_t       board_time = 0;


        /* Iterate over stop_point indexes within the route. Each one corresponds to
         * a global stop_point index. Note that the stop times array should be
         * accessed with [vj_index][jpp_offset] not [vj_index][jpp_offset].
         *
         * The iteration variable is signed to allow ending the iteration at
         * the beginning of the route, hence we decrement to 0 and need to
         * test for >= 0. An unsigned variable would always be true.
         */

        int32_t jpp_offset;

        #ifdef FEATURE_OPERATOR_FILTER
        if (req->operator != OPERATOR_UNFILTERED &&
            req->operator != jp->operator_index) continue;
        #endif

        #if 0
        if (jp_overlap) fprintf (stderr, "min time %d max time %d overlap %d \n", jp->min_time, jp->max_time, jp_overlap);
        fprintf (stderr, "journey_pattern %d has min_time %d and max_time %d. \n", jp_index, jp->min_time, jp->max_time);
        fprintf (stderr, "  actual first time: %d \n", tdata_depart(router->tdata, jp_index, 0, 0));
        fprintf (stderr, "  actual last time:  %d \n", tdata_arrive(router->tdata, jp_index, jp->n_vjs - 1, jp->n_stops - 1));
        fprintf(stderr, "  journey_pattern %d: %s;%s\n", jp_index, tdata_line_code_for_journey_pattern(router->tdata, jp_index), tdata_headsign_for_journey_pattern(router->tdata, jp_index));
        tdata_dump_journey_pattern(router->tdata, jp_index, NONE);
        #endif


        for (jpp_offset = (req->arrive_by ? jp->n_stops - 1 : 0);
                req->arrive_by ? jpp_offset >= 0 :
                jpp_offset < jp->n_stops;
                req->arrive_by ? --jpp_offset :
                                 ++jpp_offset) {

            uint32_t sp_index = journey_pattern_points[jpp_offset];
            rtime_t prev_time;
            bool attempt_board = false;
            bool forboarding = (journey_pattern_point_attributes[jpp_offset] & rsa_boarding);
            bool foralighting = (journey_pattern_point_attributes[jpp_offset] & rsa_alighting);

            #ifdef RRRR_INFO
            char buf[13];
            fprintf(stderr, "    sp %2d [%d] %c%c %s %s\n", jpp_offset,
                            sp_index,
                            forboarding ? 'B' : ' ',
                            foralighting ? 'A' : ' ',
                            btimetext(router->best_time[sp_index], buf),
                            tdata_stop_point_name_for_index (router->tdata,
                                                       sp_index));
            #endif

            if (vj_index != NONE &&
                    /* When currently on a vehicle, skip stops where
                     * alighting is not allowed at the route-point.
                     */
                    ((!forboarding && req->arrive_by) ||
                     (!foralighting && !req->arrive_by))) {
                continue;
            } else if (vj_index == NONE &&
                    /* When looking to board a vehicle, skip stops where
                     * boarding is not allowed at the route-point.
                     */
                    ((!forboarding && !req->arrive_by) ||
                     (!foralighting && req->arrive_by))) {
                continue;
            }

            #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
            /* If a stop_point in in banned_stop_points_hard, we do not want to transit
             * through this stationwe reset the current vj to NONE and skip
             * the currect stop. This effectively splits the journey_pattern in two,
             * and forces a re-board afterwards.
             */
            if (set_in_sp (req->banned_stop_points_hard, req->n_banned_stop_points_hard,
                           (spidx_t) sp_index)) {
                vj_index = NONE;
                continue;
            }
            #endif

            /* If we are not already on a vj, or if we might be able to board
             * a better vj on this journey_pattern at this location, indicate that we
             * want to search for a vj.
             */
            prev_time = states_walk_time[sp_index];

            /* Only board at placed that have been reached. */
            if (prev_time != UNREACHED) {
                if (vj_index == NONE || req->via_stop_point == sp_index) {
                    attempt_board = true;
                } else if (vj_index != NONE && req->via_stop_point != STOP_NONE &&
                                           req->via_stop_point == board_sp) {
                    attempt_board = false;
                } else {
                    rtime_t vj_stoptime = tdata_stoptime (router->tdata,
                                                        board_serviceday,
                            (jpidx_t) jp_index, vj_index, (jppidx_t) jpp_offset,
                                                        req->arrive_by);
                    if (vj_stoptime == UNREACHED) {
                        attempt_board = false;
                    } else if (req->arrive_by ? prev_time > vj_stoptime
                                              : prev_time < vj_stoptime) {
                        #ifdef RRRR_INFO
                        char buf[13];
                        fprintf (stderr, "    [reboarding here] vj = %s\n",
                                         btimetext(vj_stoptime, buf));
                        #endif
                        attempt_board = true;
                    }
                }
            }
            /* If we have not yet boarded a vj on this route, see if we can
             * board one.  Also handle the case where we hit a stop_point with an
             * existing better arrival time. */

            if (attempt_board) {
                /* Scan all vehicle_journeys to find the soonest vj that can be boarded,
                 * if any. Real-time updates can ruin FIFO ordering of vehicle_journeys
                 * within journey_patterns. Scanning through the whole list of vehicle_journeys
                 * reduces speed by ~20 percent over binary search.
                 */
                serviceday_t *best_serviceday = NULL;
                jp_vjoffset_t best_vj = NONE;
                rtime_t  best_time = (rtime_t) (req->arrive_by ? 0 : UNREACHED);

                #ifdef RRRR_INFO
                fprintf (stderr, "    attempting boarding at stop_point %d\n",
                                 sp_index);
                #endif
                #ifdef RRRR_TDATA
                tdata_dump_journey_pattern(router->tdata, jp_index, NONE);
                #endif

                if (vj_index == NONE) {
                    board_vehicle_journeys_within_days(router, req, (jpidx_t) jp_index, (jppidx_t) jpp_offset,
                            prev_time, &best_serviceday,
                            &best_vj, &best_time);
                }else{
                    reboard_vehicle_journeys_within_days(router, req, (jpidx_t) jp_index, (jppidx_t) jpp_offset,
                            board_serviceday, vj_index, prev_time, &best_serviceday,
                            &best_vj, &best_time);
                }

                if (best_vj != NONE) {
                    #ifdef RRRR_INFO
                    char buf[13];
                    fprintf(stderr, "    boarding vj %d at %s \n",
                                    best_vj, btimetext(best_time, buf));
                    #endif
                    if ((req->arrive_by ? best_time > req->time :
                                          best_time < req->time) &&
                        req->from_stop_point != ONBOARD) {
                        fprintf(stderr, "ERROR: boarded before start time, "
                                        "vj %d stop_point %d \n",
                                best_vj, sp_index);
                    } else {
                        /* TODO: use a router_state struct for all this? */
                        board_time = best_time;
                        board_sp = (spidx_t) sp_index;
                        board_jpp = (jppidx_t) jpp_offset;
                        board_serviceday = best_serviceday;
                        vj_index = best_vj;
                    }
                } else {
                    #ifdef RRRR_DEBUG_VEHICLE_JOURNEY
                    fprintf(stderr, "    no suitable vj to board.\n");
                    #endif
                }
                continue;  /*  to the next stop_point in the journey_pattern */

            /*  We have already boarded a vehicle_journey along this journey_pattern. */
            } else if (vj_index != NONE) {
                rtime_t time = tdata_stoptime (router->tdata, board_serviceday,
                                               jp_index, vj_index,
                                               (jppidx_t ) jpp_offset,
                                               !req->arrive_by);

                /* overflow due to long overnight vehicle_journeys on day 2 */
                if (time == UNREACHED) continue;

                #ifdef RRRR_DEBUG_VEHICLE_JOURNEY
                fprintf(stderr, "    on board vj %d considering time %s \n",
                                vj_index, timetext(time));
                #endif

                /* Target pruning, section 3.1 of RAPTOR paper. */
                if ((router->best_time[router->target] != UNREACHED) &&
                    (req->arrive_by ? time < router->best_time[router->target]
                                    : time > router->best_time[router->target])) {
                    #ifdef RRRR_DEBUG_VEHICLE_JOURNEY
                    fprintf(stderr, "    (target pruning)\n");
                    #endif

                    /* We cannot break out of this journey_pattern entirely,
                     * because re-boarding may occur at a later stop.
                     */
                    continue;
                }
                if ((req->time_cutoff != UNREACHED) &&
                    (req->arrive_by ? time < req->time_cutoff
                                    : time > req->time_cutoff)) {
                    continue;
                }

                /* Do we need best_time at all?
                 * Yes, because the best time may not have been found in the
                 * previous round.
                 */
                if (!((router->best_time[sp_index] == UNREACHED) ||
                       (req->arrive_by ? time > router->best_time[sp_index]
                                       : time < router->best_time[sp_index]))) {
                    #ifdef RRRR_INFO
                    fprintf(stderr, "    (no improvement)\n");
                    #endif

                    /* the current vj does not improve on the best time
                     * at this stop
                     */
                    continue;
                }
                if (time > RTIME_THREE_DAYS) {
                    /* Reserve all time past three days for
                     * special values like UNREACHED.
                     */
                } else if (req->arrive_by ? time > req->time :
                                            time < req->time) {

                    /* Wrapping/overflow. This happens due to overnight
                     * vehicle_journeys on day 2. Prune them.
                     */

                    #ifdef RRRR_DEBUG
                    fprintf(stderr, "ERROR: setting state to time before" \
                                    "start time. journey_pattern %d vj %d stop_point %d \n",
                                    jp_index, vj_index, sp_index);
                    #endif
                } else {
                    write_state(router, req, round, jp_index, vj_index,
                            sp_index, (jppidx_t) jpp_offset, time,
                            board_sp, board_jpp, board_time);

                    /*  mark stop_point for next round. */
                    bitset_set(router->updated_stop_points, sp_index);
                }
            }
        }  /*  end for (sp_index) */
    }  /*  end for (route) */

    #if RRRR_MAX_BANNED_STOP_POINTS > 0
    /* Remove the banned stops from the bitset,
     * so no transfers will happen there.
     */
    unflag_banned_stop_points(router, req);
    #endif

    /* Also updates the list of journey_patterns for next round
     * based on stops that were touched in this round.
     */
    apply_transfers(router, req, round, true,false);

    /* Initialize the stops in round 1 that were used as
     * starting points for round 0.
     */
    /*  TODO: also must be done for the hashgrid */
    if (round == 0) {
        #ifdef RRRR_FEATURE_LATLON
        initialize_transfers_full (router, 1);
        #else
        initialize_transfers (router, 1, router->origin);
        #endif
    }

     /*  TODO add arrival hashgrid timings */

    #ifdef RRRR_DEBUG
    dump_results(router);
    #endif
}

static bool initialize_origin_onboard (router_t *router, router_request_t *req) {
    /* We cannot expand the start vj into the temporary round (1)
     * during initialization because we may be able to reach the
     * destination on that starting vj.
     * We discover the previous stop_point and flag only the selected journey_pattern
     * for exploration in round 0. This would interfere with search
     * reversal, but reversal is meaningless/useless in on-board
     * depart vehicle_journeys anyway.
     */
    spidx_t sp_index;
    rtime_t stop_time;

    if (tdata_next (router, req,
                    req->onboard_journey_pattern, req->onboard_journey_pattern_vjoffset,
                    req->time, &sp_index, &stop_time) ){
        uint64_t i_state;

        req->from_stop_point = ONBOARD;

        /* Initialize the origin */
        router->origin = sp_index;
        router->best_time[router->origin] = stop_time;

        /* Set the origin stop_point in the "2nd round" */
        i_state = router->tdata->n_stop_points + router->origin;
        router->states_time[i_state]      = stop_time;
        router->states_walk_time[i_state] = stop_time;

        /* When starting on board, only flag one journey_pattern and
         * do not apply transfers, only a single walk.
         */
        bitset_clear (router->updated_stop_points);
        bitset_clear (router->updated_journey_patterns);
        bitset_set (router->updated_journey_patterns, req->onboard_journey_pattern);

        return true;
    }

    return false;
}

static bool initialize_origin_index (router_t *router, router_request_t *req) {
    uint32_t i_state;

    router->origin = (req->arrive_by ? req->to_stop_point : req->from_stop_point);

    if (router->origin == STOP_NONE) return false;

    router->best_time[router->origin] = req->time;

    /* TODO: This is a hack to communicate the origin time to itinerary
     * renderer. It would be better to just include rtime_t in request
     * structs.  eliminate this now that we have rtimes in requests.
     */
    router->states_time[router->origin] = req->time;
    bitset_clear(router->updated_stop_points);

    /* This is inefficient, as it depends on iterating over
     * a bitset with only one bit true.
     */
    bitset_set(router->updated_stop_points, router->origin);

    /* We will use round 1 to hold the initial state for round 0.
     * Round 1 must then be re-initialized before use.
     */
    i_state = router->tdata->n_stop_points + router->origin;
    router->states_time[i_state] = req->time;

    /* the rest of these should be unnecessary */
    router->states_ride_from[i_state] = STOP_NONE;
    router->states_back_journey_pattern[i_state] = NONE;
    router->states_back_vehicle_journey[i_state] = NONE;
    router->states_board_time[i_state] = UNREACHED;

    /* Apply transfers to initial state,
     * which also initializes the updated journey_patterns bitset.
     */
    apply_transfers(router, req, 1, true,true);

    return true;
}

static bool initialize_target_index (router_t *router, router_request_t *req) {
    router->target = (req->arrive_by ? req->from_stop_point : req->to_stop_point);

    return (router->target != STOP_NONE);
}

#ifdef RRRR_FEATURE_LATLON
static bool latlon_best_stop_point_index(router_t *router, router_request_t *req,
        hashgrid_result_t *hg_result) {
    double distance, best_distance = INFINITY;
    uint32_t sp_index, best_sp_index = HASHGRID_NONE;

    hashgrid_result_reset(hg_result);
    sp_index = hashgrid_result_next_filtered(hg_result, &distance);

    while (sp_index != HASHGRID_NONE) {
        uint32_t i_state;
        rtime_t extra_walktime;

        /* TODO: this is terrible. For each result we explicitly remove if it
         * is banned. While banning doesn't happen that often a more elegant
         * way would be to just overwrite the state and best_time.
         * Sadly that might not give us an accurate best_sp_index.
         */

        #if RRRR_MAX_BANNED_STOP_POINTS > 0
        /* if a stop_point is banned, we should not act upon it here */
        if (set_in_sp (req->banned_stops, req->n_banned_stops,
                       (spidx_t) sp_index)) continue;
        #endif

        #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
        /* if a stop_point is banned hard, we should not act upon it here */
        if (set_in_sp (req->banned_stop_points_hard, req->n_banned_stop_points_hard,
                       (spidx_t) sp_index)) continue;
        #endif

        i_state = router->tdata->n_stop_points + sp_index;
        extra_walktime = SEC_TO_RTIME((uint32_t)((distance * RRRR_WALK_COMP) /
                                                     req->walk_speed));

        if (req->arrive_by) {
            router->best_time[sp_index] = req->time - extra_walktime;
            router->states_time[i_state]  = req->time - extra_walktime;
        } else {
            router->best_time[sp_index] = req->time + extra_walktime;
            router->states_time[i_state]  = req->time + extra_walktime;
        }

        /*  the rest of these should be unnecessary */
        router->states_ride_from[i_state] = STOP_NONE;
        router->states_back_journey_pattern[i_state] = NONE;
        router->states_back_vehicle_journey[i_state] = NONE;
        router->states_board_time[i_state] = UNREACHED;

        bitset_set(router->updated_stop_points, sp_index);

        if (distance < best_distance) {
            best_distance = distance;
            best_sp_index = sp_index;
        }

        #ifdef RRRR_INFO
        fprintf (stderr, "%d %s %s (%.0fm)\n",
                         sp_index,
                         tdata_stop_point_id_for_index(router->tdata, sp_index),
                         tdata_stop_point_name_for_index(router->tdata, sp_index),
                         distance);
        #endif

        /* get the next potential start stop_point */
        sp_index = hashgrid_result_next_filtered(hg_result, &distance);
    }

    router->origin = best_sp_index;

    if (router->origin == STOP_NONE) return false;

    /*  TODO eliminate this now that we have rtimes in requests */
    router->states_time[router->origin] = req->time;

    /* TODO this silences a warning, but what exactly is required here */
    apply_transfers(router, req, 1, false, false);

    return true;
}
#endif

#ifdef RRRR_FEATURE_LATLON
static bool initialize_origin_latlon (router_t *router, router_request_t *req) {
    if (req->arrive_by) {
        if (req->to_latlon.lat == 0.0 &&
            req->to_latlon.lon == 0.0) {
            return false;
        }

        if (req->to_hg_result.hg == NULL) {
            coord_t coord;
            coord_from_latlon (&coord, &req->to_latlon);
            hashgrid_query (&router->tdata->hg, &req->to_hg_result,
                            coord, req->walk_max_distance);
        }
        return latlon_best_stop_point_index(router, req, &req->to_hg_result);
    } else {
        if (req->from_latlon.lat == 0.0 &&
            req->from_latlon.lon == 0.0) {
            return false;
        }

        if (req->from_hg_result.hg == NULL ) {
            coord_t coord;
            coord_from_latlon (&coord, &req->from_latlon);
            hashgrid_query (&router->tdata->hg, &req->from_hg_result,
                            coord, req->walk_max_distance);
        }
        return latlon_best_stop_point_index(router, req, &req->from_hg_result);
    }

    return false;
}

static bool initialize_target_latlon (router_t *router, router_request_t *req) {
    if (req->arrive_by) {
        if (req->from_latlon.lat == 0.0 &&
            req->from_latlon.lon == 0.0) {
            return false;
        }

        if (req->from_hg_result.hg == NULL) {
            coord_t coord;
            coord_from_latlon (&coord, &req->from_latlon);
            hashgrid_query (&router->tdata->hg, &req->from_hg_result,
                            coord, req->walk_max_distance);
        }
        hashgrid_result_reset (&req->from_hg_result);
        router->target = (spidx_t) hashgrid_result_closest (&req->from_hg_result);
    } else {
        if (req->to_latlon.lat == 0.0 &&
            req->to_latlon.lon == 0.0) {
            return false;
        }

        if (req->to_hg_result.hg == NULL ) {
            coord_t coord;
            coord_from_latlon (&coord, &req->to_latlon);
            hashgrid_query (&router->tdata->hg, &req->to_hg_result,
                            coord, req->walk_max_distance);
        }
        hashgrid_result_reset (&req->to_hg_result);
        router->target = (spidx_t) hashgrid_result_closest (&req->to_hg_result);
    }

    return (router->target != STOP_NONE);
}
#endif

static bool initialize_origin (router_t *router, router_request_t *req) {
    /* In this function we are setting up all initial required elements of the
     * routing engine we also infer what the requestee wants to do, the
     * following use cases can be observed:
     *
     * 0) from is actually onboard an existing vj
     * 1) from/to a station, req->from and/or req->to are filled
     * 2) from/to a coordinate, req->from_coord and/or req->to_coord are filled
     *
     * Given 0, we calculate the previous stop_point from an existing running vj
     * and determine the best way to the destination.
     *
     * Given 1, the user is actually at a stop_point and wants to leave from there,
     * in the second round transfers are applied which may move the user by
     * feet to a different stop. We consider the origin and destination as
     * constraint, explicitly enforced by the user.
     * "I am here, only move me if it is strictly required."
     *
     * Given 2, the user is at a location, which may be stop. We start with a
     * search around this coordinate up to a maximum walk distances,
     * configurable by the user. The first forward search starts from all
     * found locations. Based on the distance, a weight factor and the
     * walk-speed an extra time calculated and encounted for.
     * A normal search will match up to the stop_point which is the closest to the
     * destination.
     * TODO: We store all possible paths from the forward search to all
     * possible destinations.
     * The backward search uses the best times for all near by stops, again
     * the extra walk time is added. The search will now unambiously determine
     * the last possible departure.
     * From the second forward search we only search for the best plan to the
     * best destination. This plan will be marked as "best".
     *
     * De overweging om bij een alternatieve eerdere halte uit te stappen
     * en te gaan lopen baseren op het feit dat een andere, niet dezelfde
     * back_journey_pattern wordt gebruikt
     */

    /* We are starting on board a vj, not at a station. */
    if (req->onboard_journey_pattern != NONE && req->onboard_journey_pattern_vjoffset != NONE) {

        /* On-board departure only makes sense for depart-after requests. */
        if (!req->arrive_by) {
            return initialize_origin_onboard (router, req);

        } else {
            fprintf (stderr, "An arrive-by search does not make any" \
                             "sense if you are starting on-board.\n");
            return false;
        }
    } else {
        #ifdef RRRR_FEATURE_LATLON
        /* In the first two searches the LATLON search will find the best
         * values for req->to and req->from the final interation have both
         * set to the walk optimum. For the geographic optimisation to start
         * a latlon must be set and the stop_point_index must be set to NONE.
         */
        if (( req->arrive_by ? req->to_stop_point == STOP_NONE : req->from_stop_point == STOP_NONE)) {
            /* search the target based on latlon */
            return initialize_origin_latlon (router, req);
        } else
        #endif
        {
            /* search the origin based on a provided index */
            return initialize_origin_index (router, req);
        }
    }
}

static bool initialize_target (router_t *router, router_request_t *req) {
    /* In the first two searches the LATLON search will find the best
     * values for req->to and req->from the final interation have both
     * set to the walk optimum. For the geographic optimisation to start
     * a latlon must be set and the stop_point_index must be set to NONE.
     */
    #ifdef RRRR_FEATURE_LATLON
    if (( req->arrive_by ? req->from_stop_point == STOP_NONE : req->to_stop_point == STOP_NONE)) {
        /* search the target based on latlon */
        return initialize_target_latlon (router, req);
    } else
    #endif
    {
        /* search the origin based on a provided index */
        return initialize_target_index (router, req);
    }
}


bool router_route(router_t *router, router_request_t *req) {
    uint8_t i_round, n_rounds;

    /* populate router->states */
    if (!initialize_states (router)) {
        fprintf(stderr, "States could not be initialised.\n");
        return false;
    }

    /* populate router->servicedays */
    if (!initialize_servicedays (router, req)) {
        fprintf(stderr, "Serviceday could not be initialised.\n");
        return false;
    }

    #if RRRR_BANNED_JOURNEY_PATTERNS_BITMASK == 1
    /* populate router->banned_journey_patterns first, as unflag_banned_journey_patterns
     * is used via initialize_origin, apply_transfers
     */
    initialize_banned_journey_patterns (router, req);
    #endif

    /* populate router->origin */
    if (!initialize_origin (router, req)) {
        fprintf(stderr, "Search origin could not be initialised.\n");
        return false;
    }

    /* populate router->target */
    if (!initialize_target (router, req)) {
        fprintf(stderr, "Search target could not be initialised.\n");
        return false;
    }

    /* apply upper bounds (speeds up second and third reversed searches) */
    n_rounds = req->max_transfers;
    n_rounds++;
    if (n_rounds > RRRR_DEFAULT_MAX_ROUNDS) {
        n_rounds = RRRR_DEFAULT_MAX_ROUNDS;
    }

    /*  Iterate over rounds. In round N, we have made N transfers. */
    for (i_round = 0; i_round < n_rounds; ++i_round) {
        router_round(router, req, i_round);
    }

    return true;
}
