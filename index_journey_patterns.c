#include "index_journey_patterns.h"
#include "index_vehicle_journeys.h"
#include <stdlib.h>

bool index_journey_patterns (const tdata_t *td, calendar_t **jp_active,
                             rtime_t **jp_min, rtime_t **jp_max,
                             rtime_t *td_max) {
    calendar_t *active = NULL;
    rtime_t *min = NULL, *max = NULL;
    rtime_t max_time = 0;
    jpidx_t i;

    active = (calendar_t *) malloc (sizeof(calendar_t) * td->n_journey_patterns);
    min = (rtime_t *) malloc (sizeof(rtime_t) * td->n_journey_patterns);
    max = (rtime_t *) malloc (sizeof(rtime_t) * td->n_journey_patterns);
    if (!active || !min || !max) goto failure;

    i = (jpidx_t) td->n_journey_patterns;

    while (i) {
        i--;
        index_vehicle_journeys (td, i, active, min, max);
        if (max_time < max[i]) {
            max_time = max[i];
        }
    }

    *jp_active = active;
    *jp_min = min;
    *jp_max = max;
    *td_max = max_time;

    return true;

failure:
    free(active);
    free(min);
    free(max);
    return false;
}

bool index_max_time (const tdata_t *td, rtime_t *td_max) {
    jpidx_t i = (jpidx_t) td->n_journey_patterns;
    rtime_t max_time = 0;

    if (td->journey_pattern_max == NULL) return false;

    while (i) {
        i--;
        if (max_time < td->journey_pattern_max[i]) {
            max_time = td->journey_pattern_max[i];
        }
    }

    *td_max = max_time;

    return true;
}
