#include "street_network.h"

bool streetnetwork_stoppoint_durations(latlon_t *latlon, float walk_speed, uint16_t max_walk_distance, tdata_t *tdata,street_network_t *sn){
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
        sn->stop_points[sn->n_points] = (spidx_t) sp_index;
        sn->durations[sn->n_points] = SEC_TO_RTIME((uint32_t)((distance * RRRR_WALK_COMP) /
                walk_speed));
        ++sn->n_points;
        #if RRRR_DEBUG
        printf("STREET sp_index: %d, duration %d seconds\n",
        sp_index, SEC_TO_RTIME((uint32_t)((distance * RRRR_WALK_COMP) / walk_speed)));
        #endif
        /* get the next potential start stop_point */
        sp_index = hashgrid_result_next_filtered(&hg_result, &distance);
    }
    return sn->n_points > 0;
}
