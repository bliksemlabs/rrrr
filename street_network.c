#include "street_network.h"

void
street_network_init (street_network_t *sn) {
    sn->n_points = 0;
}

rtime_t street_network_duration(spidx_t sp_index, street_network_t *sn){
    spidx_t i_origin = sn->n_points;
    while (i_origin) {
        --i_origin;
        if (sn->stop_points[i_origin] == sp_index) {
            return sn->durations[i_origin];
        }
    }
    return UNREACHED;
}

bool
street_network_mark_duration_to_stop_point(street_network_t *sn,
                                           spidx_t sp_index,
                                           rtime_t duration) {
    uint32_t i = sn->n_points;

    /* A stop point already exists in the list, only duration is updated. */
    while (i) {
        --i;
        if (sn->stop_points[i] == sp_index) {
            sn->durations[i] = duration;
            return true;
        }
    }

    /* An additional duration should be added, is there enough room? */
    if (sn->n_points >= RRRR_MAX_ENTRY_EXIT_POINTS) {
        return false;
    }

    sn->stop_points[sn->n_points] = sp_index;
    sn->durations[sn->n_points] = duration;
    ++sn->n_points;

    #ifdef RRRR_DEBUG
    fprintf(stderr, "STREET sp_index: %d, duration %d seconds\n",
                    sp_index, duration);
    #endif

    return true;
}
