/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _TDATA_REALTIME_H
#define _TDATA_REALTIME_H

#include "tdata.h"

void tdata_apply_gtfsrt (tdata_t *td, uint8_t *buf, size_t len);

void tdata_apply_gtfsrt_file (tdata_t *td, char *filename);

void tdata_clear_gtfsrt (tdata_t *td);

bool tdata_alloc_expanded (tdata_t *td);

void tdata_free_expanded (tdata_t *td);

void tdata_realtime_free_trip_index (tdata_t *tdata, uint32_t trip_index);

void tdata_rt_stop_routes_append (tdata_t *tdata, uint32_t stop_index, uint32_t route_index);

void tdata_rt_stop_routes_remove (tdata_t *tdata, uint32_t stop_index, uint32_t route_index);

void tdata_apply_gtfsrt_time (TransitRealtime__TripUpdate__StopTimeUpdate *update, stoptime_t *stoptime);

#endif /* _TDATA_REALTIME_H */
