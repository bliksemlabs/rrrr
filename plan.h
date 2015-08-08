/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _PLAN_H
#define _PLAN_H

#include "rrrr_types.h"
#include "tdata.h"
#include "router_result.h"

typedef enum plan_render {
    pr_none = 0,
    pr_text = 1,
    pr_otp  = 2
} plan_render_t;

/* TODO: define structs for plan here */

bool plan_get_operators (tdata_t *td, plan_t *plan, bitset_t *bs);
uint32_t plan_render (plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen,
                      plan_render_t type);

#endif /* _PLAN_H */
