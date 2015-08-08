/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _ROUTER_RESULT_H
#define _ROUTER_RESULT_H

#include "config.h"
#include "util.h"
#include "rrrr_types.h"
#include "router.h"
#include "plan.h"

#if 0
/* Structure to temporary store abstracted plans */
typedef struct result result_t;
struct result {
    /* from stop_point index */
    spidx_t sp_from;

    /* to stop_point index */
    spidx_t sp_to;

    /* start time */
    rtime_t  t0;

    /* end time */
    rtime_t  t1;

    /* modes in trip */
    uint8_t mode;

    /* transfers in trip */
    uint8_t n_transfers;
};
#endif

bool router_result_to_plan (plan_t *plan, router_t *router, router_request_t *req);

/* return num of chars written */
uint32_t router_result_dump(router_t *router, router_request_t *req,
                            uint32_t(*render)(plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen),
                            char *buf, uint32_t buflen);

#endif
