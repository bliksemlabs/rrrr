/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"

#ifdef RRRR_FEATURE_REALTIME_EXPANDED

#include "tdata_realtime.h"
#include "radixtree.h"
#include "gtfs-realtime.pb-c.h"
#include "rrrr_types.h"
#include "stubs.h"

#include <stdio.h>
#include <alloca.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

uint16_t tdata_route_new(tdata_t *tdata, char *trip_id, uint16_t n_stops, uint16_t n_trips, uint16_t attributes, uint32_t headsign_offset, uint16_t agency_index, uint16_t shortname_index, uint16_t productcategory_index) {
    route_t *new = &tdata->routes[tdata->n_routes];
    uint32_t i_stop_index;

    new->route_stops_offset = tdata->n_route_stops;
    new->trip_ids_offset = tdata->n_trips;
    new->headsign_offset = headsign_offset;
    new->n_stops = n_stops;
    new->n_trips = n_trips;
    new->attributes = attributes;
    new->agency_index = agency_index;
    new->productcategory_index = productcategory_index;

    rxt_insert(tdata->routeid_index, trip_id, tdata->n_routes);

    strncpy(&tdata->trip_ids[tdata->n_trips * tdata->trip_ids_width], trip_id, tdata->trip_ids_width);

    tdata->n_route_stops += n_stops;
    tdata->n_route_stop_attributes += n_stops;
    tdata->n_trips += n_trips;
    tdata->n_trip_ids += n_trips;
    tdata->n_trip_active += n_trips;
    tdata->n_route_active++;

    for (i_stop_index = 0; i_stop_index < n_stops; ++i_stop_index) {
        tdata->route_stops[new->route_stops_offset + i_stop_index] = NONE;
    }

    return tdata->n_routes++;
}

/* Decodes the GTFS-RT message of lenth len in buffer buf, extracting vehicle position messages
 * and using the delay extension (1003) to update RRRR's per-trip delay information.
 */
void tdata_apply_gtfsrt (tdata_t *tdata, uint8_t *buf, size_t len) {
    size_t e;
    TransitRealtime__FeedMessage *msg;
    msg = transit_realtime__feed_message__unpack (NULL, len, buf);
    if (msg == NULL) {
        fprintf (stderr, "error unpacking incoming gtfs-rt message\n");
        return;
    }
    fprintf(stderr, "Received feed message with " ZU " entities.\n", msg->n_entity);
    for (e = 0; e < msg->n_entity; ++e) {
        TransitRealtime__TripUpdate *rt_trip_update;
        TransitRealtime__FeedEntity *rt_entity;

        rt_entity = msg->entity[e];
        if (rt_entity == NULL) goto cleanup;

        /* printf("  entity %d has id %s\n", e, entity->id); */
        rt_trip_update = rt_entity->trip_update;
        if (rt_trip_update) {
            TransitRealtime__TripDescriptor *rt_trip = rt_trip_update->trip;
            route_t *route;
            trip_t *trip;
            struct tm ltm;
            uint32_t trip_index;
            uint32_t cal_day;
            time_t epochtime;
            char buf[9];


            if (rt_trip == NULL) continue;

            trip_index = rxt_find (tdata->tripid_index, rt_trip->trip_id);
            if (trip_index == RADIX_TREE_NONE) {
                fprintf (stderr, "    trip id was not found in the radix tree.\n");
                continue;
            }

            if (rt_entity->is_deleted) {
                tdata_realtime_free_trip_index (tdata, trip_index);
                continue;
            }

            if (!rt_trip->start_date) {
                fprintf(stderr, "WARNING: not handling realtime updates without a start date!\n");
                continue;
            }

            route = tdata->routes + tdata->trip_routes[trip_index];
            trip = tdata->trips + trip_index;

            /* Take care of the realtime validity */
            memset (&ltm, 0, sizeof(struct tm));
            strncpy (buf, rt_trip->start_date, 8);
            buf[8] = '\0';
            ltm.tm_mday = strtol(&buf[6], NULL, 10);
            buf[6] = '\0';
            ltm.tm_mon  = strtol(&buf[4], NULL, 10) - 1;
            buf[4] = '\0';
            ltm.tm_mon  = strtol(&buf[0], NULL, 10) - 1900;
            ltm.tm_isdst = -1;
            epochtime = mktime(&ltm);

            cal_day = (epochtime - tdata->calendar_start_time) / SEC_IN_ONE_DAY;

            if (epochtime >= tdata->calendar_start_time && cal_day > 31 ) {
                printf("WARNING: the operational day is 32 further than our calendar!\n");
                continue;
            }

            if (rt_trip->schedule_relationship == TRANSIT_REALTIME__TRIP_DESCRIPTOR__SCHEDULE_RELATIONSHIP__CANCELED) {
                /* Apply the cancel to the schedule */
                tdata->trip_active[trip_index] &= ~(1 << cal_day);

            } else if (rt_trip->schedule_relationship == TRANSIT_REALTIME__TRIP_DESCRIPTOR__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                /* Mark in the schedule the trip is scheduled */
                tdata->trip_active[trip_index] |=  (1 << cal_day);

                if (rt_trip_update->n_stop_time_update) {
                    /* When the route changes, we have to fork a new route, and implicitly cancel this one */
                    size_t i_stu;
                    uint32_t n_stops = 0;
                    bool changed_route = false;
                    bool nodata_route = false;

                    for (i_stu = 0; i_stu < rt_trip_update->n_stop_time_update; ++i_stu) {
                        TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];

                        changed_route |= (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__ADDED ||
                                          rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED);
                        nodata_route  &= (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__NO_DATA);

                        n_stops += (rt_stop_time_update->schedule_relationship != TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED &&
                                    rt_stop_time_update->stop_id != NULL);
                    }

                    /* This entire route doesn't have any data */
                    if (nodata_route) {
                        /* If data previously was available, we should fall back to the schedule */
                        tdata_realtime_free_trip_index (tdata, trip_index);

                        /* In any case, we are done with processing this entity. */
                        continue;
                    }

                    /* If the trip has a different route, for example stops have been added or cancelled we must fork this trip into a new route */
                    if (changed_route) {
                        route_t *route_new = NULL;
                        char *trip_id_new;
                        size_t i_stu;
                        uint32_t route_index;

                        uint16_t rs = 0;
                        printf ("WARNING: this is a changed route!\n");

                        /* The idea is to fork a trip to a new route, based on the trip_id find if the trip_id already exists */
                        trip_id_new = (char *) alloca (tdata->trip_ids_width * sizeof(char));
                        trip_id_new[0] = '@';
                        strncpy(&trip_id_new[1], rt_trip->trip_id, tdata->trip_ids_width - 1);

                        route_index = rxt_find (tdata->routeid_index, trip_id_new);

                        if (route_index != RADIX_TREE_NONE) {
                            /* Fixes the case where a trip changes a second time */
                            route_new = &tdata->routes[route_index];
                            if (route_new->n_stops != n_stops) {
                                uint32_t i_stop_index;

                                printf ("WARNING: this is a changed route being CHANGED again!\n");
                                tdata->trip_stoptimes[route_new->trip_ids_offset] = (stoptime_t *) realloc(tdata->trip_stoptimes[route_new->trip_ids_offset], n_stops * sizeof(stoptime_t));
                                for (i_stop_index = route_new->n_stops; i_stop_index < n_stops; ++i_stop_index) {
                                    tdata->route_stops[route_new->route_stops_offset + i_stop_index] = NONE;
                                }
                                route_new->n_stops = n_stops;
                            }
                        }

                        if (route_new == NULL) {
                            uint16_t i_route_stops;
                            tdata->trip_active[trip_index] &= ~(1 << cal_day);

                            route_index = tdata_route_new(tdata, trip_id_new, n_stops, 1, route->attributes, route->headsign_offset, route->agency_index, route->shortname_index, route->productcategory_index);
                            route_new = &tdata->routes[route_index];
                            trip_index = route_new->trip_ids_offset;
                            tdata->trip_stoptimes[trip_index] = (stoptime_t *) malloc(route_new->n_stops * sizeof(stoptime_t));
                            tdata->trips[trip_index].stop_times_offset = tdata->n_stop_times;
                            for (i_route_stops = 0; i_route_stops < route_new->n_stops; ++i_route_stops) {
                                tdata->stop_times[tdata->trips[trip_index].stop_times_offset + i_route_stops].arrival   = UNREACHED;
                                tdata->stop_times[tdata->trips[trip_index].stop_times_offset + i_route_stops].departure = UNREACHED;
                            }

                            tdata->n_stop_times += route_new->n_stops;
                            tdata->trips[trip_index].begin_time = UNREACHED;
                            tdata->trips[trip_index].trip_attributes = trip->trip_attributes;
                            tdata->route_active[trip_index] |= (1 << cal_day);
                            tdata->trip_active[trip_index] |= (1 << cal_day);
                            tdata->trip_routes[trip_index] = route_index;
                        }

                        /* We will always overwrite the entire route, due to the problem where we have the same length, but different route */
                        for (i_stu = 0; i_stu < rt_trip_update->n_stop_time_update; ++i_stu) {
                            TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];
                            if (rt_stop_time_update->schedule_relationship != TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SKIPPED) {
                                char *stop_id = rt_stop_time_update->stop_id;
                                if (stop_id) {
                                    uint32_t stop_index = rxt_find (tdata->stopid_index, stop_id);
                                    if (tdata->route_stops[route_new->route_stops_offset + rs] != NONE && tdata->route_stops[route_new->route_stops_offset + rs] != stop_index) {
                                        tdata_rt_stop_routes_remove (tdata, tdata->route_stops[route_new->route_stops_offset + rs], route_index);
                                    }
                                    /* TODO: Should this be communicated in GTFS-RT? */
                                    tdata->route_stop_attributes[route_new->route_stops_offset + rs] = (rsa_boarding | rsa_alighting);
                                    tdata->route_stops[route_new->route_stops_offset + rs] = stop_index;
                                    tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->trip_stoptimes[trip_index][rs]);
                                    tdata_rt_stop_routes_append (tdata, stop_index, route_index);
                                    rs++;
                                }
                            }
                        }

                        route_new->min_time = tdata->trip_stoptimes[trip_index][0].arrival;
                        route_new->max_time = tdata->trip_stoptimes[trip_index][route_new->n_stops - 1].departure;

                        tdata->route_stop_attributes[route_new->route_stops_offset] = rsa_boarding;
                        tdata->route_stop_attributes[route_new->route_stops_offset + (route_new->n_stops - 1)] = rsa_alighting;

                    } else {
                        TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update_prev = NULL;
                        TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update;
                        stoptime_t *trip_times;
                        size_t i_stu;
                        uint32_t rs;

                        /* Normal case: at least one SCHEDULED or some NO_DATA stops have been observed */
                        if (tdata->trip_stoptimes[trip_index] == NULL) {
                            /* If the expanded timetable does not contain an entry yet, we are creating one */
                            tdata->trip_stoptimes[trip_index] = (stoptime_t *) malloc(route->n_stops * sizeof(stoptime_t));
                        }

                        /* The initial time-demand based schedules */
                        trip_times = tdata->stop_times + trip->stop_times_offset;

                        /* First re-initialise the old values from the schedule */
                        for (rs = 0; rs < route->n_stops; ++rs) {
                            tdata->trip_stoptimes[trip_index][rs].arrival   = trip->begin_time + trip_times[rs].arrival;
                            tdata->trip_stoptimes[trip_index][rs].departure = trip->begin_time + trip_times[rs].departure;
                        }

                        rs = 0;
                        for (i_stu = 0; i_stu < rt_trip_update->n_stop_time_update; ++i_stu) {
                            TransitRealtime__TripUpdate__StopTimeUpdate *rt_stop_time_update = rt_trip_update->stop_time_update[i_stu];

                            char *stop_id = rt_stop_time_update->stop_id;
                            uint32_t stop_index = rxt_find (tdata->stopid_index, stop_id);
                            uint32_t *route_stops = tdata->route_stops + route->route_stops_offset;

                            if (route_stops[rs] == stop_index) {
                                if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                                    tdata_apply_gtfsrt_time (rt_stop_time_update, &tdata->trip_stoptimes[trip_index][rs]);
                                }
                                /* In case of NO_DATA we won't do anything */
                                rs++;

                            } else {
                                /* we do not align up with the realtime messages */
                                if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED) {
                                    uint32_t propagate = rs;
                                    while (route_stops[++rs] != stop_index && rs < route->n_stops);
                                    if (route_stops[rs] == stop_index) {
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

                        /* the last StopTimeUpdate isn't the end of route_stops set SCHEDULED,
                         * and the departure has a delay, naively propagate the delay */
                        rt_stop_time_update = rt_trip_update->stop_time_update[rt_trip_update->n_stop_time_update - 1];
                        if (rt_stop_time_update->schedule_relationship == TRANSIT_REALTIME__TRIP_UPDATE__STOP_TIME_UPDATE__SCHEDULE_RELATIONSHIP__SCHEDULED &&
                            rt_stop_time_update->departure && rt_stop_time_update->departure->has_delay) {
                            for (; rs < route->n_stops; ++rs) {
                                tdata->trip_stoptimes[trip_index][rs].arrival   += SEC_TO_RTIME(rt_stop_time_update->departure->delay);
                                tdata->trip_stoptimes[trip_index][rs].departure += SEC_TO_RTIME(rt_stop_time_update->departure->delay);
                            }
                        }
                    }
                }
            }
        } else
        { continue; }
    }
cleanup:
    transit_realtime__feed_message__free_unpacked (msg, NULL);
}

void tdata_clear_gtfsrt (tdata_t *tdata) {
    uint32_t i_trip_index;
    for (i_trip_index = 0; i_trip_index < tdata->n_trips; ++i_trip_index) {
        tdata_realtime_free_trip_index (tdata, i_trip_index);
        /* TODO: we don't restore the original trip_active */
    }
}

void tdata_apply_gtfsrt_file (tdata_t *tdata, char *filename) {
    struct stat st;
    int fd;
    uint8_t *buf;

    fd = open(filename, O_RDONLY);
    if (fd == -1) fprintf(stderr, "Could not find GTFS_RT input file %s.\n",
                                  filename);

    if (stat(filename, &st) == -1) {
        fprintf(stderr, "Could not stat GTFS_RT input file %s.\n",
                        filename);
        goto fail_clean_fd;
    }

    buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "Could not map GTFS-RT input file %s.\n",
                        filename);
        goto fail_clean_fd;
    }

    tdata_apply_gtfsrt (tdata, buf, st.st_size);
    munmap (buf, st.st_size);

fail_clean_fd:
    close (fd);
}

bool tdata_alloc_expanded(tdata_t *td) {
    uint32_t i_route;
    td->trip_stoptimes = (stoptime_t **) calloc(td->n_trips, sizeof(stoptime_t *));
    td->trip_routes = (uint32_t *) malloc(td->n_trips * sizeof(uint32_t));

    if (!td->trip_stoptimes || !td->trip_routes) return false;

    for (i_route = 0; i_route < td->n_routes; ++i_route) {
        uint32_t i_trip;
        for (i_trip = 0; i_trip < td->routes[i_route].n_trips; ++i_trip) {
            td->trip_routes[td->routes[i_route].trip_ids_offset + i_trip] = i_route;
        }
    }

    td->rt_stop_routes = (list_t **) calloc(td->n_stops, sizeof(list_t *));
    if (!td->rt_stop_routes) return false;

    return true;
}

void tdata_free_expanded(tdata_t *td) {
    free (td->trip_routes);

    {
        uint32_t i_trip;
        for (i_trip = 0; i_trip < td->n_trips; ++i_trip) {
            free (td->trip_stoptimes[i_trip]);
        }

        free (td->trip_stoptimes);
    }

    {
        uint32_t i_stop;
        for (i_stop = 0; i_stop < td->n_stops; ++i_stop) {
            if (td->rt_stop_routes[i_stop]) {
                free (td->rt_stop_routes[i_stop]->list);
            }
        }

        free (td->rt_stop_routes);
    }
}

void tdata_apply_gtfsrt_time (TransitRealtime__TripUpdate__StopTimeUpdate *update, stoptime_t *stoptime) {
    if (update->arrival) {
        if (update->arrival->has_time) {
            stoptime->arrival  = epoch_to_rtime ((time_t) update->arrival->time, NULL) - RTIME_ONE_DAY;
        } else if (update->arrival->has_delay) {
            stoptime->arrival += SEC_TO_RTIME(update->arrival->delay);
        }
    }

    if (update->departure) {
        if (update->departure->has_time) {
            stoptime->departure  = epoch_to_rtime ((time_t) update->departure->time, NULL) - RTIME_ONE_DAY;
        } else if (update->departure->has_delay) {
            stoptime->departure += SEC_TO_RTIME(update->departure->delay);
        }
    }
}

void tdata_realtime_free_trip_index (tdata_t *tdata, uint32_t trip_index) {
    if (tdata->trip_stoptimes[trip_index]) {
        free(tdata->trip_stoptimes[trip_index]);
        tdata->trip_stoptimes[trip_index] = NULL;
        /* TODO: also free a forked route and the reference to it */
        /* TODO: restore original validity */
    }
}

/* rt_stop_routes store the delta to the planned stop_routes */
void tdata_rt_stop_routes_append (tdata_t *tdata,
                                  uint32_t stop_index, uint32_t route_index) {
    uint32_t i;

    if (tdata->rt_stop_routes[stop_index]) {
        for (i = 0; i < tdata->rt_stop_routes[stop_index]->len; ++i) {
            if (((uint32_t *) tdata->rt_stop_routes[stop_index]->list)[i] == route_index) return;
        }
    } else {
        tdata->rt_stop_routes[stop_index] = (list_t *) calloc(1, sizeof(list_t));
    }

    if (tdata->rt_stop_routes[stop_index]->len == tdata->rt_stop_routes[stop_index]->size) {
        tdata->rt_stop_routes[stop_index]->list = realloc(tdata->rt_stop_routes[stop_index]->list, (tdata->rt_stop_routes[stop_index]->size + 8) * sizeof(uint32_t));
        tdata->rt_stop_routes[stop_index]->size += 8;
    }

    ((uint32_t *) tdata->rt_stop_routes[stop_index]->list)[tdata->rt_stop_routes[stop_index]->len++] = route_index;
}

void tdata_rt_stop_routes_remove (tdata_t *tdata,
                                  uint32_t stop_index, uint32_t route_index) {
    uint32_t i;
    for (i = 0; i < tdata->rt_stop_routes[stop_index]->len; ++i) {
        if (((uint32_t *) tdata->rt_stop_routes[stop_index]->list)[i] == route_index) {
            tdata->rt_stop_routes[stop_index]->len--;
            ((uint32_t *) tdata->rt_stop_routes[stop_index]->list)[i] = ((uint32_t *) tdata->rt_stop_routes[stop_index]->list)[tdata->rt_stop_routes[stop_index]->len];
            return;
        }
    }
}

#endif /* RRRR_FEATURE_REALTIME_EXPANDED */
