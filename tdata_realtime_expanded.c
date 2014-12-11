/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"

#ifdef RRRR_FEATURE_REALTIME_EXPANDED

#include "tdata_realtime_expanded.h"
#include "radixtree.h"
#include "gtfs-realtime.pb-c.h"
#include "rrrr_types.h"
#include "util.h"

#include <time.h>
#include <stdio.h>
#include <alloca.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/* rt_journey_patterns_at_stop store the delta to the planned journey_patterns_at_stop */
static void tdata_rt_journey_patterns_at_stop_append(tdata_t *tdata,
        uint32_t stop_index,
        uint32_t jp_index) {
    uint32_t i;

    if (tdata->rt_journey_patterns_at_stop[stop_index]) {
        for (i = 0; i < tdata->rt_journey_patterns_at_stop[stop_index]->len; ++i) {
            if (((uint32_t *) tdata->rt_journey_patterns_at_stop[stop_index]->list)[i] ==
                    jp_index) return;
        }
    } else {
        tdata->rt_journey_patterns_at_stop[stop_index] =
            (list_t *) calloc(1, sizeof(list_t));
    }

    if (tdata->rt_journey_patterns_at_stop[stop_index]->len ==
        tdata->rt_journey_patterns_at_stop[stop_index]->size) {
        tdata->rt_journey_patterns_at_stop[stop_index]->list =
            realloc(tdata->rt_journey_patterns_at_stop[stop_index]->list,
                    (tdata->rt_journey_patterns_at_stop[stop_index]->size + 8) *
                    sizeof(uint32_t));
        tdata->rt_journey_patterns_at_stop[stop_index]->size += 8;
    }

    ((uint32_t *) tdata->rt_journey_patterns_at_stop[stop_index]->list)[tdata->rt_journey_patterns_at_stop[stop_index]->len++] = jp_index;
}

static void tdata_rt_journey_patterns_at_stop_remove(tdata_t *tdata,
        uint32_t stop_index,
        uint32_t jp_index) {
    uint32_t i;
    for (i = 0; i < tdata->rt_journey_patterns_at_stop[stop_index]->len; ++i) {
        if (((uint32_t *) tdata->rt_journey_patterns_at_stop[stop_index]->list)[i] ==
                jp_index) {
            tdata->rt_journey_patterns_at_stop[stop_index]->len--;
            ((uint32_t *) tdata->rt_journey_patterns_at_stop[stop_index]->list)[i] =
                ((uint32_t *) tdata->rt_journey_patterns_at_stop[stop_index]->list)[tdata->rt_journey_patterns_at_stop[stop_index]->len];
            return;
        }
    }
}

static void tdata_apply_gtfsrt_time (TransitRealtime__TripUpdate__StopTimeUpdate *update,
                                     stoptime_t *stoptime) {
    if (update->arrival) {
        if (update->arrival->has_time) {
            stoptime->arrival  = epoch_to_rtime ((time_t) update->arrival->time, NULL) - RTIME_ONE_DAY;
        } else if (update->arrival->has_delay) {
            stoptime->arrival += SEC_TO_RTIME(update->arrival->delay);
        }
    }

    /* not mutually exclusive */
    if (update->departure) {
        if (update->departure->has_time) {
            stoptime->departure  = epoch_to_rtime ((time_t) update->departure->time, NULL) - RTIME_ONE_DAY;
        } else if (update->departure->has_delay) {
            stoptime->departure += SEC_TO_RTIME(update->departure->delay);
        }
    }
}

static void tdata_realtime_free_trip_index (tdata_t *tdata, uint32_t trip_index) {
    if (tdata->trip_stoptimes[trip_index]) {
        free(tdata->trip_stoptimes[trip_index]);
        tdata->trip_stoptimes[trip_index] = NULL;
        /* TODO: also free a forked journey_pattern and the reference to it */
        /* TODO: restore original validity
         * TODO: test if below works
         */
        tdata->trip_active[trip_index] = tdata->trip_active_orig[trip_index];
/*        memcpy (&(tdata->trip_active[trip_index]),
                &(tdata->trip_active_orig[trip_index]),
                sizeof(calendar_t));*/
    }
}


/* Our datastructure requires us to commit on a fixed number of
 * trips and a fixed number of stops in the journey_pattern.
 * Generally speaking, when a new journey_pattern is dynamically added,
 * we will have only one trip, a list of stops in the journey_pattern.
 *
 * This call will preallocate, and fill the journey_pattern and matching
 * trips, and wire them together. Stops and times will be added
 * later, they can be completely dynamic up to the allocated
 * length.
 *
 * For all our data tables this implies that we have up to
 * n elements (tdata->n_journey_pattern_points, tdata->n_trips) in memory
 * and will extend this by the number of elements this journey_pattern
 * will contain.
 */

static uint32_t tdata_new_journey_pattern(tdata_t *tdata, char *trip_ids,
        uint16_t n_stops, uint16_t n_trips,
        uint16_t attributes, uint32_t headsign_offset,
        uint16_t agency_index, uint16_t line_code_index,
        uint16_t productcategory_index) {
    journey_pattern_t *new;
    uint32_t journey_pattern_point_offset = tdata->n_journey_pattern_points;
    uint32_t stop_times_offset = tdata->n_stop_times;
    uint32_t trip_index        = tdata->n_trips;
    uint16_t i_stop;
    uint16_t i_trip;

    new = &tdata->journey_patterns[tdata->n_journey_patterns];

    new->journey_pattern_point_offset = journey_pattern_point_offset;
    new->trip_ids_offset = trip_index;
    new->headsign_offset = headsign_offset;
    new->n_stops = n_stops;
    new->n_trips = n_trips;
    new->attributes = attributes;
    new->agency_index = agency_index;
    new->productcategory_index = productcategory_index;
    new->line_code_index = line_code_index;

    tdata->trips[trip_index].stop_times_offset = stop_times_offset;

    for (i_stop = 0; i_stop < n_stops; ++i_stop) {
        tdata->journey_pattern_points[journey_pattern_point_offset] = NONE;
        journey_pattern_point_offset++;

        /* Initialise the timetable */
        tdata->stop_times[stop_times_offset].arrival   = UNREACHED;
        tdata->stop_times[stop_times_offset].departure = UNREACHED;
        stop_times_offset++;

    }

    /* append the allocated trip_ids to the array */
    strncpy(&tdata->trip_ids[tdata->n_trips * tdata->trip_ids_width],
            trip_ids, tdata->trip_ids_width * n_trips);

    /* add the last journey_pattern index to the lookup table */
    for (i_trip = 0; i_trip < n_trips; ++i_trip) {
        tdata->trip_stoptimes[trip_index] = (stoptime_t *) malloc(n_stops * sizeof(stoptime_t));

        for (i_stop = 0; i_stop < n_stops; ++i_stop) {
            /* Initialise the realtime stoptimes */
            tdata->trip_stoptimes[trip_index][i_stop].arrival   = UNREACHED;
            tdata->trip_stoptimes[trip_index][i_stop].departure = UNREACHED;
        }

        tdata->trips[trip_index].begin_time = UNREACHED;
        tdata->trips_in_journey_pattern[trip_index] = tdata->n_journey_patterns;
        trip_index++;

        rxt_insert(tdata->lineid_index,
                   &trip_ids[i_trip * tdata->trip_ids_width],
                   tdata->n_journey_patterns);
    }

    /* housekeeping in tdata: increment for each new element */
    tdata->n_stop_times += n_stops;
    tdata->n_journey_pattern_points += n_stops;
    tdata->n_journey_pattern_point_attributes += n_stops;
    tdata->n_trips += n_trips;
    tdata->n_trip_ids += n_trips;
    tdata->n_trip_active += n_trips;
    tdata->n_journey_pattern_active++;

    return tdata->n_journey_patterns++;
}

void tdata_apply_stop_time_update (tdata_t *tdata, uint32_t jp_index, uint32_t trip_index, TransitRealtime__TripUpdate *rt_trip_update) {
    uint32_t journey_pattern_point_offset = tdata->journey_patterns[jp_index].journey_pattern_point_offset;
    uint32_t rs = 0;
    uint32_t i_stu;

    /* We will always overwrite the entire journey_pattern, because we cannot
     * tell if the journey_pattern is the same length, but consists of different
     * stops.
     */
    for (i_stu = 0;
         i_stu < rt_trip_update->n_stop_time_update;
         ++i_stu) {
        TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];
        if (rt_stop_time_update->schedule_relationship != TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED) {
            char *stop_id = rt_stop_time_update->stop_id;
            if (stop_id) {

                uint32_t stop_index = rxt_find (tdata->stopid_index, stop_id);
                if (tdata->journey_pattern_points[journey_pattern_point_offset] != stop_index &&
                    tdata->journey_pattern_points[journey_pattern_point_offset] != NONE) {
                    tdata_rt_journey_patterns_at_stop_remove(tdata, tdata->journey_pattern_points[journey_pattern_point_offset], jp_index);
                }
                /* TODO: Should this be communicated in GTFS-RT? */
                tdata->journey_pattern_point_attributes[journey_pattern_point_offset] = (rsa_boarding | rsa_alighting);
                tdata->journey_pattern_points[journey_pattern_point_offset] = stop_index;
                tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->trip_stoptimes[trip_index][rs]);
                tdata_rt_journey_patterns_at_stop_append(tdata, stop_index, jp_index);
                journey_pattern_point_offset++;
                rs++;
            }
        }
    }

    /* update the last stop to be alighting only */
    tdata->journey_pattern_point_attributes[--journey_pattern_point_offset] = rsa_alighting;

    /* update the first stop to be boarding only */
    journey_pattern_point_offset = tdata->journey_patterns[jp_index].journey_pattern_point_offset;
    tdata->journey_pattern_point_attributes[journey_pattern_point_offset] = rsa_boarding;
}

static void tdata_realtime_changed_journey_pattern(tdata_t *tdata, uint32_t trip_index, int16_t cal_day, uint16_t n_stops, TransitRealtime__TripUpdate *rt_trip_update) {
    TransitRealtime__TripDescriptor *rt_trip = rt_trip_update->trip;
    journey_pattern_t *jp_new = NULL;
    char *trip_id_new;
    uint32_t jp_index;

    /* Don't ever continue if we found that n_stops == 0. */
    if (n_stops == 0) return;

    #ifdef RRRR_DEBUG
    fprintf (stderr, "WARNING: this is a changed journey_pattern!\n");
    #endif

    /* The idea is to fork a trip to a new journey_pattern, based on
     * the trip_id find if the trip_id already exists
     */
    trip_id_new = (char *) alloca (tdata->trip_ids_width * sizeof(char));
    trip_id_new[0] = '@';
    strncpy(&trip_id_new[1], rt_trip->trip_id, tdata->trip_ids_width - 1);

    jp_index = rxt_find (tdata->lineid_index, trip_id_new);

    if (jp_index != RADIX_TREE_NONE) {
        /* Fixes the case where a trip changes a second time */
        jp_new = &tdata->journey_patterns[jp_index];
        if (jp_new->n_stops != n_stops) {
            uint32_t i_stop_index;

            #ifdef RRRR_DEBUG
            fprintf (stderr, "WARNING: this is changed trip %s being CHANGED again!\n", trip_id_new);
            #endif
            tdata->trip_stoptimes[jp_new->trip_ids_offset] = (stoptime_t *) realloc(tdata->trip_stoptimes[jp_new->trip_ids_offset], n_stops * sizeof(stoptime_t));

            /* Only initialises if the length of the list increased */
            for (i_stop_index = jp_new->n_stops;
                 i_stop_index < n_stops;
                 ++i_stop_index) {
                tdata->journey_pattern_points[jp_new->journey_pattern_point_offset + i_stop_index] = NONE;
            }
            jp_new->n_stops = n_stops;
        }
    }

    if (jp_new == NULL) {
        journey_pattern_t *jp = tdata->journey_patterns + tdata->trips_in_journey_pattern[trip_index];
        trip_t  *trip = tdata->trips + trip_index;
        uint16_t i_trips;

        /* remove the planned trip from the operating day */
        tdata->trip_active[trip_index] &= ~(1 << cal_day);

        /* we fork a new journey_pattern with all of its old properties
         * having one single trip, which is the modification.
         */
        jp_index = tdata_new_journey_pattern(tdata, trip_id_new, n_stops, 1,
                jp->attributes,
                jp->headsign_offset,
                jp->agency_index,
                jp->line_code_index,
                jp->productcategory_index);

        jp_new = &(tdata->journey_patterns[jp_index]);

        /* In the new trip_index we restore the original values
         * NOTE: if we would have allocated multiple trips this
         * should be a for loop over n_trips.
         */
        trip_index = jp_new->trip_ids_offset;

        for (i_trips = 0; i_trips < 1; ++i_trips) {
            tdata->trips[trip_index].trip_attributes = trip->trip_attributes;
            tdata->journey_pattern_active[trip_index] |= (1 << cal_day);
            tdata->trip_active[trip_index] |= (1 << cal_day);
            trip_index++;
        }

        /* Restore the first trip_index again */
        trip_index = jp_new->trip_ids_offset;
    }

    tdata_apply_stop_time_update (tdata, jp_index, trip_index, rt_trip_update);

    /* being blissfully naive, a journey_pattern having only one trip,
     * will have the same start and end time as its trip
     */
    jp_new->min_time = tdata->trip_stoptimes[jp_new->trip_ids_offset][0].arrival;
    jp_new->max_time = tdata->trip_stoptimes[trip_index][jp_new->n_stops - 1].departure;

}

static void tdata_realtime_journey_pattern_type(TransitRealtime__TripUpdate *rt_trip_update, uint16_t *n_stops, bool *changed_jp, bool *nodata_jp) {
    size_t i_stu;
    *n_stops = 0;
    *changed_jp = false;
    *nodata_jp = true;

    for (i_stu = 0;
         i_stu < rt_trip_update->n_stop_time_update;
         ++i_stu) {
        TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];

        *changed_jp |= (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__ADDED ||
                           rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED);
        *nodata_jp &= (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__NO_DATA);

        *n_stops += (rt_stop_time_update->schedule_relationship != TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED &&
                     rt_stop_time_update->stop_id != NULL);
    }
}

static void tdata_realtime_apply_tripupdates (tdata_t *tdata, uint32_t trip_index, TransitRealtime__TripUpdate *rt_trip_update) {
    TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update_prev = NULL;
    TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update;
    journey_pattern_t *jp;
    trip_t *trip;
    stoptime_t *trip_times;
    size_t i_stu;
    uint32_t rs;

    jp = tdata->journey_patterns + tdata->trips_in_journey_pattern[trip_index];
    trip = tdata->trips + trip_index;

    /* Normal case: at least one SCHEDULED or some NO_DATA
     * stops have been observed
     */
    if (tdata->trip_stoptimes[trip_index] == NULL) {
        /* If the expanded timetable does not contain an entry yet, we are creating one */
        tdata->trip_stoptimes[trip_index] = (stoptime_t *) malloc(jp->n_stops * sizeof(stoptime_t));
    }

    /* The initial time-demand based schedules */
    trip_times = tdata->stop_times + trip->stop_times_offset;

    /* First re-initialise the old values from the schedule */
    for (rs = 0; rs < jp->n_stops; ++rs) {
        tdata->trip_stoptimes[trip_index][rs].arrival   = trip->begin_time + trip_times[rs].arrival;
        tdata->trip_stoptimes[trip_index][rs].departure = trip->begin_time + trip_times[rs].departure;
    }

    rs = 0;
    for (i_stu = 0; i_stu < rt_trip_update->n_stop_time_update; ++i_stu) {
        uint32_t *journey_pattern_points = tdata->journey_pattern_points + jp->journey_pattern_point_offset;
        char *stop_id;
        uint32_t stop_index;

        rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];
        stop_id = rt_stop_time_update->stop_id;
        stop_index = rxt_find (tdata->stopid_index, stop_id);


        if (journey_pattern_points[rs] == stop_index) {
            if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->trip_stoptimes[trip_index][rs]);
            }
            /* In case of NO_DATA we won't do anything */
            rs++;

        } else {
            /* we do not align up with the realtime messages */
            if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                uint32_t propagate = rs;
                while (journey_pattern_points[++rs] != stop_index && rs < jp->n_stops);
                if (journey_pattern_points[rs] == stop_index) {
                    if (rt_stop_time_update_prev) {
                        if (rt_stop_time_update_prev->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED &&
                            rt_stop_time_update_prev->departure && rt_stop_time_update_prev->departure->has_delay) {
                            for (; propagate < rs; ++propagate) {
                                tdata->trip_stoptimes[trip_index][propagate].arrival   += SEC_TO_RTIME(rt_stop_time_update_prev->departure->delay);
                                tdata->trip_stoptimes[trip_index][propagate].departure += SEC_TO_RTIME(rt_stop_time_update_prev->departure->delay);
                            }
                        }
                    }
                    tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->trip_stoptimes[trip_index][rs]);
                } else {
                    /* we couldn't find the stop at all */
                    rs = propagate;
                }
                rs++;
            }
        }

        rt_stop_time_update_prev = rt_stop_time_update;
    }

    /* the last StopTimeUpdate isn't the end of journey_pattern_points set SCHEDULED,
        * and the departure has a delay, naively propagate the delay */
    rt_stop_time_update = rt_trip_update->stop_time_update[rt_trip_update->n_stop_time_update - 1];
    if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED &&
        rt_stop_time_update->departure && rt_stop_time_update->departure->has_delay) {
        for (; rs < jp->n_stops; ++rs) {
            tdata->trip_stoptimes[trip_index][rs].arrival   += SEC_TO_RTIME(rt_stop_time_update->departure->delay);
            tdata->trip_stoptimes[trip_index][rs].departure += SEC_TO_RTIME(rt_stop_time_update->departure->delay);
        }
    }
}


/* PUBLIC INTERFACES */

bool tdata_alloc_expanded(tdata_t *td) {
    uint32_t i_jp;
    td->trip_stoptimes = (stoptime_t **) calloc(td->n_trips, sizeof(stoptime_t *));
    td->trips_in_journey_pattern = (uint32_t *) malloc(td->n_trips * sizeof(uint32_t));

    if (!td->trip_stoptimes || !td->trips_in_journey_pattern) return false;

    for (i_jp = 0; i_jp < td->n_journey_patterns; ++i_jp) {
        uint32_t i_trip;
        for (i_trip = 0; i_trip < td->journey_patterns[i_jp].n_trips; ++i_trip) {
            td->trips_in_journey_pattern[td->journey_patterns[i_jp].trip_ids_offset + i_trip] =
                    i_jp;
        }
    }

    td->rt_journey_patterns_at_stop = (list_t **) calloc(td->n_stops, sizeof(list_t *));

    td->trip_active_orig = (calendar_t *) malloc(td->n_trips *
                                                 sizeof(calendar_t));

    td->journey_pattern_active_orig = (calendar_t *) malloc(td->n_trips *
                                                  sizeof(calendar_t));

    memcpy (td->trip_active_orig, td->trip_active,
            td->n_trips * sizeof(calendar_t));

    memcpy (td->journey_pattern_active_orig, td->journey_pattern_active,
            td->n_journey_patterns * sizeof(calendar_t));

    if (!td->rt_journey_patterns_at_stop) return false;

    return true;
}

void tdata_free_expanded(tdata_t *td) {
    free (td->trips_in_journey_pattern);

    if (td->trip_stoptimes) {
        uint32_t i_trip;
        for (i_trip = 0; i_trip < td->n_trips; ++i_trip) {
            free (td->trip_stoptimes[i_trip]);
        }

        free (td->trip_stoptimes);
    }

    if (td->rt_journey_patterns_at_stop) {
        uint32_t i_stop;
        for (i_stop = 0; i_stop < td->n_stops; ++i_stop) {
            if (td->rt_journey_patterns_at_stop[i_stop]) {
                free (td->rt_journey_patterns_at_stop[i_stop]->list);
            }
        }

        free (td->rt_journey_patterns_at_stop);
    }

    free (td->trip_active_orig);
    free (td->journey_pattern_active_orig);
}



/* Decodes the GTFS-RT message of lenth len in buffer buf, extracting vehicle
 * position messages and using the delay extension (1003) to update RRRR's
 * per-trip delay information.
 */
void tdata_apply_gtfsrt_tripupdates (tdata_t *tdata, uint8_t *buf, size_t len) {
    size_t e;
    TransitRealtime__FeedMessage *msg;
    msg = transit_realtime__feed_message__unpack (NULL, len, buf);
    if (msg == NULL) {
        fprintf (stderr, "error unpacking incoming gtfs-rt message\n");
        return;
    }
    #ifdef RRRR_DEBUG
    fprintf(stderr, "Received feed message with " ZU " entities.\n", msg->n_entity);
    #endif
    for (e = 0; e < msg->n_entity; ++e) {
        TransitRealtime__TripUpdate *rt_trip_update;
        TransitRealtime__FeedEntity *rt_entity;

        rt_entity = msg->entity[e];
        if (rt_entity == NULL) goto cleanup;

        #ifdef RRRR_DEBUG
        fprintf(stderr, "  entity %lu has id %s\n", (unsigned long) e, rt_entity->id);
        #endif
        rt_trip_update = rt_entity->trip_update;
        if (rt_trip_update) {
            TransitRealtime__TripDescriptor *rt_trip = rt_trip_update->trip;
            struct tm ltm;
            uint32_t trip_index;
            time_t epochtime;
            int16_t cal_day;
            char buf[9];


            if (rt_trip == NULL) continue;

            trip_index = rxt_find (tdata->tripid_index, rt_trip->trip_id);
            if (trip_index == RADIX_TREE_NONE) {
                #ifdef RRRR_DEBUG
                fprintf (stderr, "    trip id was not found in the radix tree.\n");
                #endif
                continue;
            }

            if (rt_entity->is_deleted) {
                tdata_realtime_free_trip_index (tdata, trip_index);
                continue;
            }

            if (!rt_trip->start_date) {
                #ifdef RRRR_DEBUG
                fprintf(stderr, "WARNING: not handling realtime updates without a start date!\n");
                #endif
                continue;
            }

            /* Take care of the realtime validity */
            memset (&ltm, 0, sizeof(struct tm));
            strncpy (buf, rt_trip->start_date, 8);
            buf[8] = '\0';
            ltm.tm_mday = strtol(&buf[6], NULL, 10);
            buf[6] = '\0';
            ltm.tm_mon  = strtol(&buf[4], NULL, 10) - 1;
            buf[4] = '\0';
            ltm.tm_year = strtol(&buf[0], NULL, 10) - 1900;
            ltm.tm_isdst = -1;
            epochtime = mktime(&ltm);

            cal_day = (epochtime - tdata->calendar_start_time) / SEC_IN_ONE_DAY;

            if (cal_day < 0 || cal_day > 31 ) {
                #ifdef RRRR_DEBUG
                fprintf(stderr, "WARNING: the operational day is 32 further than our calendar!\n");
                #endif

                #ifndef RRRR_FAKE_REALTIME
                continue;
                #endif
            }

            if (rt_trip->schedule_relationship ==
                TRANSIT_REALTIME__TRIP_DESCRIPTOR__SCHEDULE_RELATIONSHIP__CANCELED) {
                /* Apply the cancel to the schedule */
                tdata->trip_active[trip_index] &= ~(1 << cal_day);

            } else if (rt_trip->schedule_relationship ==
                       TRANSIT_REALTIME__TRIP_DESCRIPTOR__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                /* Mark in the schedule the trip is scheduled */
                tdata->trip_active[trip_index] |=  (1 << cal_day);

                if (rt_trip_update->n_stop_time_update) {
                    uint16_t n_stops;
                    bool changed_jp;
                    bool nodata_jp;

                    tdata_realtime_journey_pattern_type(rt_trip_update, &n_stops, &changed_jp, &nodata_jp);

                    /* Don't ever continue if we found that n_stops == 0,
                     * this entire journey_pattern doesn't have any data.
                     */
                    if (nodata_jp || n_stops == 0) {
                        /* If data previously was available, we should fall
                         * back to the schedule
                         */
                        tdata_realtime_free_trip_index (tdata, trip_index);

                        /* In any case, we are done with processing this entity. */
                        continue;
                    }

                    /* If the trip has a different journey_pattern, for example stops
                     * have been added or cancelled we must fork this trip
                     * into a new journey_pattern
                     */
                    else if (changed_jp) {
                        tdata_realtime_changed_journey_pattern(tdata, trip_index, cal_day, n_stops, rt_trip_update);

                    } else {
                        tdata_realtime_apply_tripupdates (tdata, trip_index, rt_trip_update);

                    }
                }
            }
        } else
        { continue; }
    }
cleanup:
    transit_realtime__feed_message__free_unpacked (msg, NULL);
}

void tdata_apply_gtfsrt_tripupdates_file (tdata_t *tdata, char *filename) {
    struct stat st;
    int fd;
    uint8_t *buf;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Could not open GTFS_RT input file %s.\n", filename);
        return;
    }

    if (stat(filename, &st) == -1) {
        fprintf(stderr, "Could not stat GTFS_RT input file %s.\n", filename);
        goto fail_clean_fd;
    }

    buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "Could not map GTFS-RT input file %s.\n", filename);
        goto fail_clean_fd;
    }

    tdata_apply_gtfsrt_tripupdates (tdata, buf, st.st_size);
    munmap (buf, st.st_size);

fail_clean_fd:
    close (fd);
}

void tdata_clear_gtfsrt (tdata_t *tdata) {
    uint32_t i_trip_index;
    for (i_trip_index = 0; i_trip_index < tdata->n_trips; ++i_trip_index) {
        tdata_realtime_free_trip_index (tdata, i_trip_index);
    }
    memcpy (tdata->trip_active, tdata->trip_active_orig,
            tdata->n_trips * sizeof(calendar_t));
    memcpy (tdata->journey_pattern_active, tdata->journey_pattern_active_orig,
            tdata->n_trips * sizeof(calendar_t));
}

#else
   void tdata_gtfsrt_not_available() {}
#endif /* RRRR_FEATURE_REALTIME_EXPANDED */
