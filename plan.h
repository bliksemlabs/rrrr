/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _PLAN_H
#define _PLAN_H

#include "rrrr_types.h"
#include "tdata.h"
#include "router_result.h"

/* TODO: define structs for plan here */

bool plan_get_operators (tdata_t *td, plan_t *plan, bitset_t *bs);

#endif /* _PLAN_H */
