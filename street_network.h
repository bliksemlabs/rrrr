#ifndef _STREET_NETWORK_H
#define _STREET_NETWORK_H

#include "rrrr_types.h"
#include "tdata.h"

bool streetnetwork_stoppoint_durations(latlon_t *latlon, float walk_speed, uint16_t max_walk_distance, tdata_t *tdata,street_network_t *sn);

#endif

