/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "street_network.h"

bool street_network_stoppoint_durations(latlon_t *latlon, float walk_speed, uint16_t max_walk_distance, tdata_t *tdata, street_network_t *sn){
    coord_t coord;
    double distance;
    uint32_t sp_index;
    hashgrid_result_t hg_result;
    coord_from_latlon(&coord, latlon);
    sn->n_points = 0;
    hashgrid_query (&tdata->hg, &hg_result, coord, max_walk_distance);
    hashgrid_result_reset(&hg_result);

    sp_index = hashgrid_result_next_filtered(&hg_result, &distance);
    while (sp_index != HASHGRID_NONE) {
        rtime_t duration = SEC_TO_RTIME((uint32_t)((distance * RRRR_WALK_COMP) /
                walk_speed));
        street_network_mark_duration_to_stop_point(sn, (spidx_t) sp_index, duration);
        /* get the next potential start stop_point */
        sp_index = hashgrid_result_next_filtered(&hg_result, &distance);
    }
    return sn->n_points > 0;
}
