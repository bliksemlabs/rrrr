/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "plan.h"

bool plan_get_operators (tdata_t *td, plan_t *plan, bitset_t *bs) {
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
