/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "plan.h"

#ifdef RRRR_FEATURE_RENDER_TEXT
#include "plan_render_text.h"
#endif

#ifdef RRRR_FEATURE_RENDER_OTP
#include "plan_render_otp.h"
#endif

uint32_t plan_render (plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen,
                      plan_render_t type) {
#if !defined (RRRR_FEATURE_RENDER_TEXT) && !defined (RRRR_FEATURE_RENDER_TEXT)
    UNUSED(plan);
    UNUSED(tdata);
    UNUSED(buf);
    UNUSED(buflen);
#endif

    switch (type) {
    case pr_text:
#ifdef RRRR_FEATURE_RENDER_TEXT
        return plan_render_text(plan, tdata, buf, buflen);
#endif
    case pr_otp:
#ifdef RRRR_FEATURE_RENDER_OTP
        return plan_render_otp(plan, tdata, buf, buflen);
#endif
    case pr_none:
        break;
    }

    return 0;
}

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
