#include "index_vehicle_journeys.h"

void index_vehicle_journeys (const tdata_t *td, uint32_t i, calendar_t *active, rtime_t *min, rtime_t *max) {
    journey_pattern_t *jp = &td->journey_patterns[i];
    calendar_t mask = 0;
    vjidx_t v1, v2;

    v1 = jp->vj_index;
    v2 = v1 + jp->n_vjs;

    /* this assumes we have a sorted list of vehicle journeys */
    min[i] = td->vjs[v1].begin_time;
    max[i] = td->vjs[v2 - 1].begin_time + td->stop_times[td->vjs[v2 - 1].stop_times_offset + jp->n_stops - 1].departure;

    while (v2 > v1) {
        rtime_t new_max;
        v2--;
        mask |= td->vj_active[v2];
        new_max = td->vjs[v2].begin_time + td->stop_times[td->vjs[v2].stop_times_offset + jp->n_stops - 1].departure;
        
        /* it seems that the source data isn't sorted, or realtime is applied */
        if (min[i] > td->vjs[v1].begin_time) min[i] = td->vjs[v1].begin_time;
        if (max[i] < new_max) max[i] = new_max;
    }

    active[i] = mask;
}
