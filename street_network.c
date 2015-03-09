#include "street_network.h"

void
street_network_init (street_network_t *sn) {
    sn->n_points = 0;
}

bool street_network_mark_duration_to_stop_point(street_network_t *sn, spidx_t sp_index, rtime_t duration){
    int32_t i = sn->n_points;
    while (i){
        --i;
        if (sn->stop_points[i] == sp_index){
            sn->durations[i] = duration;
            return true;
        }
    }
    if (i >= RRRR_MAX_ENTRY_EXIT_POINTS){
        return false;
    }
    sn->stop_points[sn->n_points] = sp_index;
    sn->durations[sn->n_points] = duration;
    ++sn->n_points;
    #ifdef RRRR_DEBUG
    printf("STREET sp_index: %d, duration %d seconds\n", sp_index, duration);
    #endif
    return true;
}
