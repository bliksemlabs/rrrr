/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "plan_render.h"

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
