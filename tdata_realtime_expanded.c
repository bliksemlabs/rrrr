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
#include "string_pool.h"

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

/* rt_journey_patterns_at_stop_point store the delta to the planned journey_patterns_at_stop_point */
static void tdata_rt_journey_patterns_at_stop_point_append(tdata_t *tdata,
        uint32_t sp_index,
        uint32_t jp_index) {
    uint32_t i;

    if (tdata->rt_journey_patterns_at_stop_point[sp_index]) {
        for (i = 0; i < tdata->rt_journey_patterns_at_stop_point[sp_index]->len; ++i) {
            if (((uint32_t *) tdata->rt_journey_patterns_at_stop_point[sp_index]->list)[i] ==
                    jp_index) return;
        }
    } else {
        tdata->rt_journey_patterns_at_stop_point[sp_index] =
            (list_t *) calloc(1, sizeof(list_t));
    }

    if (tdata->rt_journey_patterns_at_stop_point[sp_index]->len ==
        tdata->rt_journey_patterns_at_stop_point[sp_index]->size) {
        tdata->rt_journey_patterns_at_stop_point[sp_index]->list =
            realloc(tdata->rt_journey_patterns_at_stop_point[sp_index]->list,
                    sizeof(uint32_t) *
                    (tdata->rt_journey_patterns_at_stop_point[sp_index]->size + 8));
        tdata->rt_journey_patterns_at_stop_point[sp_index]->size += 8;
    }

    ((uint32_t *) tdata->rt_journey_patterns_at_stop_point[sp_index]->list)[tdata->rt_journey_patterns_at_stop_point[sp_index]->len++] = jp_index;
}

static void tdata_rt_journey_patterns_at_stop_point_remove(tdata_t *tdata,
        uint32_t sp_index,
        uint32_t jp_index) {
    uint32_t i;
    for (i = 0; i < tdata->rt_journey_patterns_at_stop_point[sp_index]->len; ++i) {
        if (((uint32_t *) tdata->rt_journey_patterns_at_stop_point[sp_index]->list)[i] ==
                jp_index) {
            tdata->rt_journey_patterns_at_stop_point[sp_index]->len--;
            ((uint32_t *) tdata->rt_journey_patterns_at_stop_point[sp_index]->list)[i] =
                ((uint32_t *) tdata->rt_journey_patterns_at_stop_point[sp_index]->list)[tdata->rt_journey_patterns_at_stop_point[sp_index]->len];
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

static void tdata_realtime_free_vj_index(tdata_t *tdata, uint32_t vj_index) {
    if (tdata->vj_stoptimes[vj_index]) {
        free(tdata->vj_stoptimes[vj_index]);
        tdata->vj_stoptimes[vj_index] = NULL;
        /* TODO: also free a forked journey_pattern and the reference to it */
        /* TODO: restore original validity
         * TODO: test if below works
         */
        tdata->vj_active[vj_index] = tdata->vj_active_orig[vj_index];
/*        memcpy (&(tdata->vj_active[vj_index]),
                &(tdata->vj_active_orig[vj_index]),
                sizeof(calendar_t));*/
    }
}


/* Our datastructure requires us to commit on a fixed number of
 * vehicle_journeys and a fixed number of stop_points in the journey_pattern.
 * Generally speaking, when a new journey_pattern is dynamically added,
 * we will have only one vj, a list of stop_points in the journey_pattern.
 *
 * This call will preallocate, and fill the journey_pattern and matching
 * vehicle_journeys, and wire them together. stop_points and times will be added
 * later, they can be completely dynamic up to the allocated
 * length.
 *
 * For all our data tables this implies that we have up to
 * n elements (tdata->n_journey_pattern_points, tdata->n_vjs) in memory
 * and will extend this by the number of elements this journey_pattern
 * will contain.
 */

static uint32_t tdata_new_journey_pattern(tdata_t *tdata, char *vj_ids,
        uint16_t n_sp, uint16_t n_vjs,
        uint16_t attributes, uint16_t route_index) {
    journey_pattern_t *new;
    uint32_t journey_pattern_point_offset = tdata->n_journey_pattern_points;
    uint32_t stop_times_offset = tdata->n_stop_times;
    uint32_t vj_index = tdata->n_vjs;
    spidx_t sp_index;
    uint16_t i_vj;

    new = &tdata->journey_patterns[tdata->n_journey_patterns];

    new->journey_pattern_point_offset = journey_pattern_point_offset;
    new->vj_offset = vj_index;
    new->n_stops = n_sp;
    new->n_vjs = n_vjs;
    new->attributes = attributes;
    new->route_index = route_index;

    tdata->vjs[vj_index].stop_times_offset = stop_times_offset;

    for (sp_index = 0; sp_index < n_sp; ++sp_index) {
        tdata->journey_pattern_points[journey_pattern_point_offset] = NONE;
        journey_pattern_point_offset++;

        /* Initialise the timetable */
        tdata->stop_times[stop_times_offset].arrival   = UNREACHED;
        tdata->stop_times[stop_times_offset].departure = UNREACHED;
        stop_times_offset++;

    }

    /* add the last journey_pattern index to the lookup table */
    for (i_vj = 0; i_vj < n_vjs; ++i_vj) {
        /* TODO: this doesn't handle multiple vjs! */
        tdata->vj_ids[vj_index] = string_pool_append (tdata->string_pool, &tdata->n_string_pool, tdata->stringpool_index, vj_ids);

        tdata->vj_stoptimes[vj_index] = (stoptime_t *) malloc(sizeof(stoptime_t) * n_sp);

        for (sp_index = 0; sp_index < n_sp; ++sp_index) {
            /* Initialise the realtime stoptimes */
            tdata->vj_stoptimes[vj_index][sp_index].arrival   = UNREACHED;
            tdata->vj_stoptimes[vj_index][sp_index].departure = UNREACHED;
        }

        tdata->vjs[vj_index].begin_time = UNREACHED;
        tdata->vjs_in_journey_pattern[vj_index] = tdata->n_journey_patterns;
        vj_index++;

        /* TODO: this doesn't handle multiple vjs! */
        radixtree_insert(tdata->lineid_index, vj_ids,
                         tdata->n_journey_patterns);
    }

    /* housekeeping in tdata: increment for each new element */
    tdata->n_stop_times += n_sp;
    tdata->n_journey_pattern_points += n_sp;
    tdata->n_journey_pattern_point_attributes += n_sp;
    tdata->n_journey_pattern_point_headsigns += n_sp;
    tdata->n_vjs += n_vjs;
    tdata->n_vj_ids += n_vjs;
    tdata->n_vj_active += n_vjs;
    tdata->n_journey_pattern_active++;

    return tdata->n_journey_patterns++;
}

static void tdata_apply_stop_time_update (tdata_t *tdata, uint32_t jp_index, uint32_t vj_index, TransitRealtime__TripUpdate *rt_trip_update) {
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

                uint32_t sp_index = radixtree_find (tdata->stop_point_id_index, stop_id);
                if (tdata->journey_pattern_points[journey_pattern_point_offset] != sp_index &&
                    tdata->journey_pattern_points[journey_pattern_point_offset] != NONE) {
                    tdata_rt_journey_patterns_at_stop_point_remove(tdata, tdata->journey_pattern_points[journey_pattern_point_offset], jp_index);
                }
                /* TODO: Should this be communicated in GTFS-RT? */
                tdata->journey_pattern_point_attributes[journey_pattern_point_offset] = (rsa_boarding | rsa_alighting);
                /* TODO: We dont know headsign, set to string_idx of last stop in jp?? */
                tdata->journey_pattern_point_headsigns[journey_pattern_point_offset] = 0;
                tdata->journey_pattern_points[journey_pattern_point_offset] = sp_index;
                tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->vj_stoptimes[vj_index][rs]);
                tdata_rt_journey_patterns_at_stop_point_append(tdata, sp_index, jp_index);
                journey_pattern_point_offset++;
                rs++;
            }
        }
    }

    /* update the last stop_point to be alighting only */
    tdata->journey_pattern_point_attributes[--journey_pattern_point_offset] = rsa_alighting;

    /* update the first stop_point to be boarding only */
    journey_pattern_point_offset = tdata->journey_patterns[jp_index].journey_pattern_point_offset;
    tdata->journey_pattern_point_attributes[journey_pattern_point_offset] = rsa_boarding;
}

static void tdata_realtime_changed_journey_pattern(tdata_t *tdata, uint32_t vj_index, int16_t cal_day, uint16_t n_sp, TransitRealtime__TripUpdate *rt_trip_update) {
    TransitRealtime__TripDescriptor *rt_trip = rt_trip_update->trip;
    journey_pattern_t *jp_new = NULL;
    char *vj_id_new;
    size_t len;
    uint32_t jp_index;

    /* Don't ever continue if we found that n_sp == 0. */
    if (n_sp == 0) return;

    #ifdef RRRR_DEBUG
    fprintf (stderr, "WARNING: this is a changed journey_pattern!\n");
    #endif

    /* The idea is to fork a vj to a new journey_pattern, based on
     * the vehicle_journey id to find if the vehicle_journey id already exists
     */

    len = strlen(rt_trip->trip_id);
    vj_id_new = (char *) alloca (len + 2);
    vj_id_new[0] = '@';
    strncpy(&vj_id_new[1], rt_trip->trip_id, len);

    jp_index = radixtree_find (tdata->lineid_index, vj_id_new);

    if (jp_index != RADIXTREE_NONE) {
        /* Fixes the case where a vj changes a second time */
        jp_new = &tdata->journey_patterns[jp_index];
        if (jp_new->n_stops != n_sp) {
            uint32_t sp_index;

            #ifdef RRRR_DEBUG
            fprintf (stderr, "WARNING: this is changed vehicle_journey %s being CHANGED again!\n", vj_id_new);
            #endif
            tdata->vj_stoptimes[jp_new->vj_offset] = (stoptime_t *) realloc(tdata->vj_stoptimes[jp_new->vj_offset], sizeof(stoptime_t) * n_sp);

            /* Only initialises if the length of the list increased */
            for (sp_index = jp_new->n_stops;
                 sp_index < n_sp;
                 ++sp_index) {
                tdata->journey_pattern_points[jp_new->journey_pattern_point_offset + sp_index] = NONE;
            }
            jp_new->n_stops = n_sp;
        }
    }

    if (jp_new == NULL) {
        journey_pattern_t *jp = tdata->journey_patterns + tdata->vjs_in_journey_pattern[vj_index];
        vehicle_journey_t  *vj = tdata->vjs + vj_index;
        uint16_t i_vj;

        /* remove the planned vj from the operating day */
        tdata->vj_active[vj_index] &= ~(1 << cal_day);

        /* we fork a new journey_pattern with all of its old properties
         * having one single vj, which is the modification.
         */
        jp_index = tdata_new_journey_pattern(tdata, vj_id_new, n_sp, 1,
                jp->attributes,
                jp->route_index);

        jp_new = &(tdata->journey_patterns[jp_index]);

        /* In the new vj_index we restore the original values
         * NOTE: if we would have allocated multiple vehicle_journeys this
         * should be a for loop over n_vjs.
         */
        vj_index = jp_new->vj_offset;

        for (i_vj = 0; i_vj < 1; ++i_vj) {
            tdata->vjs[vj_index].vj_attributes = vj->vj_attributes;
            tdata->journey_pattern_active[vj_index] |= (1 << cal_day);
            tdata->vj_active[vj_index] |= (1 << cal_day);
            vj_index++;
        }

        /* Restore the first vj_index again */
        vj_index = jp_new->vj_offset;
    }

    tdata_apply_stop_time_update (tdata, jp_index, vj_index, rt_trip_update);

    /* being blissfully naive, a journey_pattern having only one vehicle_journey,
     * will have the same start and end time as its vehicle_journey
     */
    jp_new->min_time = tdata->vj_stoptimes[jp_new->vj_offset][0].arrival;
    jp_new->max_time = tdata->vj_stoptimes[vj_index][jp_new->n_stops - 1].departure;

}

static void tdata_realtime_journey_pattern_type(TransitRealtime__TripUpdate *rt_trip_update, uint16_t *n_sp, bool *changed_jp, bool *nodata_jp) {
    size_t i_stu;
    *n_sp = 0;
    *changed_jp = false;
    *nodata_jp = true;

    for (i_stu = 0;
         i_stu < rt_trip_update->n_stop_time_update;
         ++i_stu) {
        TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];

        *changed_jp |= (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__ADDED ||
                           rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED);
        *nodata_jp &= (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__NO_DATA);

        *n_sp += (rt_stop_time_update->schedule_relationship != TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED &&
                     rt_stop_time_update->stop_id != NULL);
    }
}

static void tdata_realtime_apply_tripupdates (tdata_t *tdata, uint32_t vj_index, TransitRealtime__TripUpdate *rt_trip_update) {
    TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update_prev = NULL;
    TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update;
    journey_pattern_t *jp;
    vehicle_journey_t *vj;
    stoptime_t *vj_stoptimes;
    size_t i_stu;
    uint32_t rs;

    jp = tdata->journey_patterns + tdata->vjs_in_journey_pattern[vj_index];
    vj = tdata->vjs + vj_index;

    /* Normal case: at least one SCHEDULED or some NO_DATA
     * stops have been observed
     */
    if (tdata->vj_stoptimes[vj_index] == NULL) {
        /* If the expanded timetable does not contain an entry yet, we are creating one */
        tdata->vj_stoptimes[vj_index] = (stoptime_t *) malloc(sizeof(stoptime_t) * jp->n_stops);
    }

    /* The initial time-demand based schedules */
    vj_stoptimes = tdata->stop_times + vj->stop_times_offset;

    /* First re-initialise the old values from the schedule */
    for (rs = 0; rs < jp->n_stops; ++rs) {
        tdata->vj_stoptimes[vj_index][rs].arrival   = vj->begin_time + vj_stoptimes[rs].arrival;
        tdata->vj_stoptimes[vj_index][rs].departure = vj->begin_time + vj_stoptimes[rs].departure;
    }

    rs = 0;
    for (i_stu = 0; i_stu < rt_trip_update->n_stop_time_update; ++i_stu) {
        spidx_t *journey_pattern_points = tdata->journey_pattern_points + jp->journey_pattern_point_offset;
        char *stop_id;
        uint32_t sp_index;

        rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];
        stop_id = rt_stop_time_update->stop_id;
        sp_index = radixtree_find (tdata->stop_point_id_index, stop_id);


        if (journey_pattern_points[rs] == sp_index) {
            if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->vj_stoptimes[vj_index][rs]);
            }
            /* In case of NO_DATA we won't do anything */
            rs++;

        } else {
            /* we do not align up with the realtime messages */
            if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                uint32_t propagate = rs;
                while (journey_pattern_points[++rs] != sp_index && rs < jp->n_stops);
                if (journey_pattern_points[rs] == sp_index) {
                    if (rt_stop_time_update_prev) {
                        if (rt_stop_time_update_prev->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED &&
                            rt_stop_time_update_prev->departure && rt_stop_time_update_prev->departure->has_delay) {
                            for (; propagate < rs; ++propagate) {
                                tdata->vj_stoptimes[vj_index][propagate].arrival   += SEC_TO_RTIME(rt_stop_time_update_prev->departure->delay);
                                tdata->vj_stoptimes[vj_index][propagate].departure += SEC_TO_RTIME(rt_stop_time_update_prev->departure->delay);
                            }
                        }
                    }
                    tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->vj_stoptimes[vj_index][rs]);
                } else {
                    /* we couldn't find the stop_point at all */
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
            tdata->vj_stoptimes[vj_index][rs].arrival   += SEC_TO_RTIME(rt_stop_time_update->departure->delay);
            tdata->vj_stoptimes[vj_index][rs].departure += SEC_TO_RTIME(rt_stop_time_update->departure->delay);
        }
    }
}


/* PUBLIC INTERFACES */

bool tdata_alloc_expanded(tdata_t *td) {
    uint32_t i_jp;
    td->vj_stoptimes = (stoptime_t **) calloc(td->n_vjs, sizeof(stoptime_t *));
    td->vjs_in_journey_pattern = (uint32_t *) malloc(sizeof(uint32_t) * td->n_vjs);

    if (!td->vj_stoptimes || !td->vjs_in_journey_pattern) return false;

    for (i_jp = 0; i_jp < td->n_journey_patterns; ++i_jp) {
        uint32_t i_vj;
        for (i_vj = 0; i_vj < td->journey_patterns[i_jp].n_vjs; ++i_vj) {
            td->vjs_in_journey_pattern[td->journey_patterns[i_jp].vj_offset + i_vj] =
                    i_jp;
        }
    }

    td->rt_journey_patterns_at_stop_point = (list_t **) calloc(td->n_stop_points, sizeof(list_t *));

    td->vj_active_orig = (calendar_t *) malloc(sizeof(calendar_t) * td->n_vjs);

    td->journey_pattern_active_orig = (calendar_t *) malloc(sizeof(calendar_t) * td->n_vjs);

    memcpy (td->vj_active_orig, td->vj_active,
            sizeof(calendar_t) * td->n_vjs);

    memcpy (td->journey_pattern_active_orig, td->journey_pattern_active,
            sizeof(calendar_t) * td->n_journey_patterns);

    if (!td->rt_journey_patterns_at_stop_point) return false;

    return true;
}

void tdata_free_expanded(tdata_t *td) {
    free (td->vjs_in_journey_pattern);

    if (td->vj_stoptimes) {
        uint32_t i_vj;
        for (i_vj = 0; i_vj < td->n_vjs; ++i_vj) {
            free (td->vj_stoptimes[i_vj]);
        }

        free (td->vj_stoptimes);
    }

    if (td->rt_journey_patterns_at_stop_point) {
        uint32_t i_sp;
        for (i_sp = 0; i_sp < td->n_stop_points; ++i_sp) {
            if (td->rt_journey_patterns_at_stop_point[i_sp]) {
                free (td->rt_journey_patterns_at_stop_point[i_sp]->list);
            }
        }

        free (td->rt_journey_patterns_at_stop_point);
    }

    free (td->vj_active_orig);
    free (td->journey_pattern_active_orig);
}



/* Decodes the GTFS-RT message of lenth len in buffer buf, extracting vehicle
 * position messages and using the delay extension (1003) to update RRRR's
 * per-vj delay information.
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
            uint32_t vj_index;
            time_t epochtime;
            int16_t cal_day;
            char buf[9];


            if (rt_trip == NULL) continue;

            vj_index = radixtree_find (tdata->vjid_index, rt_trip->trip_id);
            if (vj_index == RADIXTREE_NONE) {
                #ifdef RRRR_DEBUG
                fprintf (stderr, "    trip id was not found in the radix tree.\n");
                #endif
                continue;
            }

            if (rt_entity->is_deleted) {
                tdata_realtime_free_vj_index(tdata, vj_index);
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
            ltm.tm_mday = (int) strtol(&buf[6], NULL, 10);
            buf[6] = '\0';
            ltm.tm_mon  = (int) strtol(&buf[4], NULL, 10) - 1;
            buf[4] = '\0';
            ltm.tm_year = (int) strtol(&buf[0], NULL, 10) - 1900;
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
                tdata->vj_active[vj_index] &= ~(1 << cal_day);

            } else if (rt_trip->schedule_relationship ==
                       TRANSIT_REALTIME__TRIP_DESCRIPTOR__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                /* Mark in the schedule the vj is scheduled */
                tdata->vj_active[vj_index] |=  (1 << cal_day);

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
                        tdata_realtime_free_vj_index(tdata, vj_index);

                        /* In any case, we are done with processing this entity. */
                        continue;
                    }

                    /* If the vj has a different journey_pattern, for example stops
                     * have been added or cancelled we must fork this vj
                     * into a new journey_pattern
                     */
                    else if (changed_jp) {
                        tdata_realtime_changed_journey_pattern(tdata, vj_index, cal_day, n_stops, rt_trip_update);

                    } else {
                        tdata_realtime_apply_tripupdates (tdata, vj_index, rt_trip_update);

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
    uint32_t i_vj_index;
    for (i_vj_index = 0; i_vj_index < tdata->n_vjs; ++i_vj_index) {
        tdata_realtime_free_vj_index(tdata, i_vj_index);
    }
    memcpy (tdata->vj_active, tdata->vj_active_orig,
            sizeof(calendar_t) * tdata->n_vjs);
    memcpy (tdata->journey_pattern_active, tdata->journey_pattern_active_orig,
            sizeof(calendar_t) * tdata->n_vjs);
}

#else
   void tdata_gtfsrt_not_available() {}
#endif /* RRRR_FEATURE_REALTIME_EXPANDED */
