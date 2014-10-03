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

    printf("Received feed message with " ZU " entities.\n", msg->n_entity);
    for (e = 0; e < msg->n_entity; ++e) {
        size_t ie;
        TransitRealtime__FeedEntity *entity;
        TransitRealtime__Alert *alert;

        entity = msg->entity[e];
        if (entity == NULL) goto cleanup;
        /* printf("  entity %d has id %s\n", e, entity->id); */

        alert = entity->alert;
        if (alert == NULL) goto cleanup;

        for (ie = 0; ie < alert->n_informed_entity; ++ie) {
            TransitRealtime__EntitySelector *informed_entity = alert->informed_entity[ie];
            if (!informed_entity) continue;

            if (informed_entity->route_id) {
                uint32_t route_index = rxt_find (tdata->routeid_index, informed_entity->route_id);
                if (route_index == RADIX_TREE_NONE) {
                     printf ("    route id was not found in the radix tree.\n");
                }
                memcpy (informed_entity->route_id, &route_index, sizeof(route_index));
            }

            if (informed_entity->stop_id) {
                uint32_t stop_index = rxt_find (tdata->stopid_index, informed_entity->stop_id);
                if (stop_index == RADIX_TREE_NONE) {
                     printf ("    stop id was not found in the radix tree.\n");
                }
                memcpy (informed_entity->stop_id, &stop_index, sizeof(stop_index));
            }

            if (informed_entity->trip && informed_entity->trip->trip_id) {
                uint32_t trip_index = rxt_find (tdata->tripid_index, informed_entity->trip->trip_id);
                if (trip_index == RADIX_TREE_NONE) {
                    printf ("    trip id was not found in the radix tree.\n");
                }
                memcpy (informed_entity->trip->trip_id, &trip_index, sizeof(trip_index));
            }
        }
    }

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
    if (fd == -1) fprintf(stderr, "Could not find GTFS_RT input file %s.\n",
                                  filename);


    if (stat(filename, &st) == -1) {
        fprintf(stderr, "Could not stat GTFS_RT input file %s.\n",
                        filename);
        goto clean_fd;
    }

    buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "Could not map GTFS-RT input file %s.\n",
                        filename);
        goto clean_fd;
    }

    tdata_apply_gtfsrt_alerts (tdata, buf, st.st_size);
    munmap (buf, st.st_size);

clean_fd:
    close (fd);
}

void tdata_clear_gtfsrt_alerts (tdata_t *tdata) {
    if (tdata->alerts) {
        transit_realtime__feed_message__free_unpacked (tdata->alerts, NULL);
        tdata->alerts = NULL;
    }
}

#endif /* RRRR_FEATURE_REALTIME_ALERTS */
