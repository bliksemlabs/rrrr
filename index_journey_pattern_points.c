#include "index_journey_pattern_points.h"
#include <stdlib.h>

/* To create the journey patterns at stop list we know
 * that the list equals the length of all journeypatterns
 * by ordering the stopid, journeypatternid, we can dedup
 * in a second round.
 */

typedef struct jp_s_index jp_s_index_t;
struct jp_s_index {
    jpidx_t j;
    spidx_t s;
};

static int
jp_s_cmp(const void *elem1, const void *elem2) {
    const jp_s_index_t *i1 = (const jp_s_index_t *) elem1;
    const jp_s_index_t *i2 = (const jp_s_index_t *) elem2;

    return ((i1->s - i2->s) << 16) + (i1->j - i2->j);
}

static jp_s_index_t*
journey_pattern_stop_index_ordered (tdata_t *td, uint32_t *n) {
    jp_s_index_t *idx;
    uint32_t n_idx;
    jpidx_t jp_index;

    /* allocate the slackspace to compute the index */
    idx = malloc(td->n_journey_pattern_points * sizeof(jp_s_index_t));
    if (!idx) return NULL;

    /* copy the journey pattern points from the timetable */
    n_idx = 0;
    for (jp_index = 0; jp_index < td->n_journey_patterns; ++jp_index) {
        uint32_t jpp_index;
        journey_pattern_t *jp = td->journey_patterns + jp_index;

        for (jpp_index = jp->journey_pattern_point_offset;
             jpp_index < (jp->journey_pattern_point_offset + jp->n_stops);
             jpp_index++) {
            idx[n_idx].j = jp_index;
            idx[n_idx].s = td->journey_pattern_points[jpp_index];
            n_idx++;
        }
    }

    /* sort the journey pattern points in sp_idx, jp_idx order */
    qsort (idx, n_idx, sizeof(jp_s_index_t), jp_s_cmp);

    *n = n_idx;
    return idx;
}

bool index_journey_pattern_points (tdata_t *td, jpidx_t **jps, uint32_t *n, uint32_t **jpaspo) {
    jp_s_index_t *idx = NULL;
    jpidx_t *journey_patterns_at_stop = NULL;
    uint32_t *journey_patterns_at_stop_point_offset = NULL;
    uint32_t i, i2, n_idx, n_journey_patterns_at_stop;
    spidx_t sp_index;

    idx = journey_pattern_stop_index_ordered (td, &n_idx);
    if (!idx) return false;

    /* compute the length of the final allocation */
    /* TODO: validate the 0 case: no journey patterns */
    n_journey_patterns_at_stop = 1;
    for (i = 1; i < n_idx; i++) {
        n_journey_patterns_at_stop += (idx[i - 1].s != idx[i].s) || (idx[i - 1].j != idx[i].j);
    }

    #if 0
    printf ("%u %u\n", td->n_journey_patterns_at_stop, td->n_stop_points);
    #endif

    /* allocate the final index */
    journey_patterns_at_stop = (jpidx_t *) malloc (sizeof (jpidx_t) * n_journey_patterns_at_stop);
    if (!journey_patterns_at_stop) goto failure;

    /* allocate the new stop_point_offset */
    journey_patterns_at_stop_point_offset = (uint32_t *) malloc (sizeof (uint32_t) * td->n_stop_points);
    if (!journey_patterns_at_stop_point_offset) goto failure;

    i = 0;
    i2 = 0;
    journey_patterns_at_stop[i2] = idx[i].j;
    /* we fill the stop points we have no data for with next available data */
    for (sp_index = 0; sp_index <= idx[0].s; sp_index++) journey_patterns_at_stop_point_offset[sp_index] = i2;
    i2++;
    i++;

    /* What does this loop do?
     * i is our the index for out unduplicated list.
     * i2 is our deduplicated index for the final list
     */
    for (; i < n_idx; i++) {
        if (idx[i - 1].s != idx[i].s) {
            journey_patterns_at_stop[i2] = idx[i].j;
            for (; sp_index <= idx[i].s; sp_index++) journey_patterns_at_stop_point_offset[sp_index] = i2;
            i2++;
        } else if (idx[i - 1].j != idx[i].j) {
            journey_patterns_at_stop[i2] = idx[i].j;
            i2++;
        }
    }

    /* our list is done, fill up remaining stops in the list with the highest
     * jp_index found, this makes the length of the list zero  */
    for (; sp_index <= td->n_stop_points; sp_index++) journey_patterns_at_stop_point_offset[sp_index] = i2;

    *jps = journey_patterns_at_stop;
    *n = n_journey_patterns_at_stop;
    *jpaspo = journey_patterns_at_stop_point_offset;

    free (idx);
    return true;

failure:
    free (idx);
    free (journey_patterns_at_stop);
    free (journey_patterns_at_stop_point_offset);
    return false;
}
