/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* router_dump.h */

#ifndef _ROUTER_DUMP_H
#define _ROUTER_DUMP_H

#include "rrrr_types.h"
#include "router.h"

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#if 0
void router_state_dump (router_t *router);
#endif
bool stop_is_reached(router_t *router, uint32_t stop_index);
#if 0
void dump_results(router_t *router);
#endif
void day_mask_dump (uint32_t mask);
void service_day_dump (struct service_day *sd);

#if 0
void dump_vehicle_journeys(router_t *router);
#endif

#endif /* _ROUTER_DUMP_H */
