/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "plan.h"

bool itinerary_init (itinerary_t *itin) {
    itin->n_legs = 0;
    itin->n_rides = 0;

    return true;
}

bool plan_init (plan_t *plan) {
    plan->n_itineraries = 0;

    return true;
}

static int
compareItineraries(const void *elem1, const void *elem2) {
    const itinerary_t *i1 = (const itinerary_t *) elem1;
    const itinerary_t *i2 = (const itinerary_t *) elem2;

    return ((i1->legs[0].t0 - i2->legs[0].t0) << 16) +
            i1->legs[i1->n_legs - 1].t1 - i2->legs[i2->n_legs - 1].t1;
}

void plan_sort (plan_t *plan) {
    qsort(&plan->itineraries, plan->n_itineraries,
          sizeof(itinerary_t), compareItineraries);
}

bool plan_get_operators (plan_t *plan, tdata_t *td, bitset_t *bs) {
    uint8_t n_itineraries;
    n_itineraries = plan->n_itineraries;

    while (n_itineraries) {
        uint8_t n_legs;
        --n_itineraries;
        n_legs = plan->itineraries[n_itineraries].n_legs;

        while (n_legs) {
            jpidx_t jp;
            --n_legs;
            jp = plan->itineraries[n_itineraries].legs[n_legs].journey_pattern;
            if (jp < WALK) {
                bitset_set (bs, tdata_operator_idx_for_journey_pattern(td, jp));
            }
        }
    }

    return true;
}
