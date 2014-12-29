/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"

#ifdef RRRR_FEATURE_REALTIME_ALERTS

#include "tdata_realtime_alerts.h"
#include "radixtree.h"
#include "gtfs-realtime.pb-c.h"
#include "rrrr_types.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

void tdata_apply_gtfsrt_alerts (tdata_t *tdata, uint8_t *buf, size_t len) {
    size_t e;
    TransitRealtime__FeedMessage *msg = transit_realtime__feed_message__unpack (NULL, len, buf);
    if (msg == NULL) {
        fprintf (stderr, "error unpacking incoming gtfs-rt message\n");
        return;
    }

    #ifdef RRRR_DEBUG
    fprintf(stderr, "Received feed message with " ZU " entities.\n",
                    msg->n_entity);
    #endif

    for (e = 0; e < msg->n_entity; ++e) {
        size_t ie;
        TransitRealtime__FeedEntity *entity;
        TransitRealtime__Alert *alert;

        entity = msg->entity[e];
        if (entity == NULL) goto cleanup;
        #ifdef RRRR_REALTIME
        fprintf(stderr, "  entity %d has id %s\n", e, entity->id);
        #endif

        alert = entity->alert;
        if (alert == NULL) goto cleanup;

        for (ie = 0; ie < alert->n_informed_entity; ++ie) {
            TransitRealtime__EntitySelector *informed_entity =
                                                    alert->informed_entity[ie];
            if (!informed_entity) continue;

            if (informed_entity->route_id) {
                uint32_t jp_index = radixtree_find (tdata->lineid_index,
                                                    informed_entity->route_id);
                /*TODO This only applies the alert to one of the journey_patterns in the line/route.*/
                #ifdef RRRR_DEBUG
                if (jp_index == RADIXTREE_NONE) {
                     fprintf (stderr,
                     "    route id was not found in the radix tree.\n");
                }
                #endif

                *(informed_entity->route_id) = jp_index;
            }

            if (informed_entity->stop_id) {
                uint32_t sp_index = radixtree_find (tdata->stop_point_id_index,
                                                      informed_entity->stop_id);
                #ifdef RRRR_DEBUG
                if (sp_index == RADIXTREE_NONE) {
                     fprintf (stderr,
                     "    stop id was not found in the radix tree.\n");
                }
                #endif

                *(informed_entity->stop_id) = sp_index;
            }

            if (informed_entity->trip && informed_entity->trip->trip_id) {
                uint32_t trip_index = radixtree_find (tdata->vjid_index,
                                                      informed_entity->trip->trip_id);
                #ifdef RRRR_DEBUG
                if (trip_index == RADIXTREE_NONE) {
                     fprintf (stderr,
                    "    trip id was not found in the radix tree.\n");
                }
                #endif

                *(informed_entity->trip->trip_id) = trip_index;
            }
        }
    }

    /* clean up the existing alerts */
    tdata_clear_gtfsrt_alerts (tdata);

    /* assign the new alerts */
    tdata->alerts = msg;
    return;

cleanup:
    transit_realtime__feed_message__free_unpacked (msg, NULL);
}


void tdata_apply_gtfsrt_alerts_file (tdata_t *tdata, char *filename) {
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
        fprintf(stderr, "Could not mmap GTFS-RT input file %s.\n", filename);
        goto fail_clean_fd;
    }

    tdata_apply_gtfsrt_alerts (tdata, buf, st.st_size);
    munmap (buf, st.st_size);

fail_clean_fd:
    close (fd);
}

void tdata_clear_gtfsrt_alerts (tdata_t *tdata) {
    if (tdata->alerts) {
        transit_realtime__feed_message__free_unpacked (tdata->alerts, NULL);
        tdata->alerts = NULL;
    }
}
#else
void tdata_gtfsrt_alerts_not_available() {}
#endif /* RRRR_FEATURE_REALTIME_ALERTS */
