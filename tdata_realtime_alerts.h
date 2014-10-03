/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _TDATA_REALTIME_ALERTS_H
#define _TDATA_REALTIME_ALERTS_H

#include "tdata.h"

void tdata_apply_gtfsrt_alerts (tdata_t *td, uint8_t *buf, size_t len);

void tdata_apply_gtfsrt_alerts_file (tdata_t *td, char *filename);

void tdata_clear_gtfsrt_alerts (tdata_t *td);

#endif /* _TDATA_REALTIME_ALERTS_H */
