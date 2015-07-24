#include "index_journey_patterns.h"
#include "index_vehicle_journeys.h"
#include <stdlib.h>

bool index_journey_patterns (const tdata_t *td, calendar_t **jp_active, rtime_t **jp_min, rtime_t **jp_max) {
    calendar_t *active = NULL;
    rtime_t *min = NULL, *max = NULL;
    jpidx_t i;

    active = (calendar_t *) malloc (sizeof(calendar_t) * td->n_journey_patterns);
    min = (rtime_t *) malloc (sizeof(rtime_t) * td->n_journey_patterns);
    max = (rtime_t *) malloc (sizeof(rtime_t) * td->n_journey_patterns);
    if (!active || !min || !max) goto failure;

    i = (jpidx_t) td->n_journey_patterns;

    while (i) {
        i--;
        index_vehicle_journeys (td, i, active, min, max);
    }

    *jp_active = active;
    *jp_min = min;
    *jp_max = max;

    return true;

failure:
    free(active);
    free(min);
    free(max);
    return false;
}
