#include "config.h"
#include "rrrr_types.h"
#include "router_result.h"
#include "router_request.h"
#include <stdio.h>
#include <string.h>

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
             * place.
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
uint32_t
plan_render_text(plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen) {
    char *b = buf;
    char *b_end = buf + buflen;
    struct tm ltm;
    time_t date = router_request_to_date (&plan->req, tdata, &ltm);

    if ((plan->req.optimise & o_all) == o_all) {
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
        if ((plan->req.optimise & o_transfers) == o_transfers) {
           b = plan_render_itinerary (plan->itineraries, tdata, date, b, b_end);
        }

        /* only render the last itinerary,
         * which has the most rides and is the shortest in time
         */
        if ((plan->req.optimise & o_shortest) == o_shortest) {
            b = plan_render_itinerary (&plan->itineraries[plan->n_itineraries - 1], tdata, date, b, b_end);
        }
    }
    *b = '\0';
    return b - buf;
}
