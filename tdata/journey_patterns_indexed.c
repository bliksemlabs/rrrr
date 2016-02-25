#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "journey_patterns_indexed.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_journey_patterns_indexed_init (tdata_journey_patterns_indexed_t *jpsi)
{
    /* indices */
    jpsi->min_time = NULL;
    jpsi->max_time = NULL;
    jpsi->journey_pattern_active = NULL;
    jpsi->journey_patterns_at_stop = NULL;

    return ret_ok;
}

ret_t
tdata_journey_patterns_indexed_mrproper (tdata_journey_patterns_indexed_t *jpsi)
{
    if (jpsi->min_time)
        free (jpsi->min_time);

    if (jpsi->max_time)
        free (jpsi->max_time);

    if (jpsi->journey_pattern_active)
        free (jpsi->journey_pattern_active);

    if (jpsi->journey_patterns_at_stop)
        free (jpsi->journey_patterns_at_stop);

    return tdata_journey_patterns_indexed_init (jpsi);
}

ret_t
tdata_journey_patterns_indexed_index (tdata_journey_patterns_indexed_t *jpsi)
{
    void *p;

    /* If it is a new buffer, take memory and return
     */
    if (jpsi->min_time == NULL) {
        /* indices */
        jpsi->min_time = (rtime_t *) calloc (jpsi->jps.size, sizeof(rtime_t));
        jpsi->max_time = (rtime_t *) calloc (jpsi->jps.size, sizeof(rtime_t));
        jpsi->journey_pattern_active = (calendar_t *) calloc (jpsi->jps.size, sizeof(calendar_t));

        if (unlikely (jpsi->min_time == NULL ||
                      jpsi->max_time == NULL ||
                      jpsi->journey_pattern_active == NULL)) {
            return ret_nomem;
        }

        /* start indexing */

        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (jpsi->min_time, sizeof(rtime_t) * jpsi->jps.size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jpsi->min_time = (rtime_t *) p;

    p = realloc (jpsi->max_time, sizeof(rtime_t) * jpsi->jps.size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jpsi->max_time = (rtime_t *) p;

    p = realloc (jpsi->journey_pattern_active, sizeof(calendar_t) * jpsi->jps.size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jpsi->journey_pattern_active = (calendar_t *) p;

    /* start indexing */

    return ret_ok;
}
