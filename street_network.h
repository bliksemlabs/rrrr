/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _STREET_NETWORK_H
#define _STREET_NETWORK_H

#include "rrrr_types.h"
#include "tdata.h"

void street_network_init (street_network_t *sn);
bool streetnetwork_stoppoint_durations(latlon_t *latlon, float walk_speed, uint16_t max_walk_distance, tdata_t *tdata,street_network_t *sn);
bool street_network_mark_duration_to_stop_point(street_network_t *sn, spidx_t sp_index, rtime_t duration);
rtime_t street_network_duration(spidx_t sp_index, street_network_t *sn);
#endif

