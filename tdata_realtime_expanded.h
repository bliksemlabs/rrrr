/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _TDATA_REALTIME_H
#define _TDATA_REALTIME_H

#ifdef RRRR_FEATURE_REALTIME_EXPANDED

#include "tdata.h"

bool tdata_alloc_expanded (tdata_t *td);

void tdata_free_expanded (tdata_t *td);

void tdata_apply_gtfsrt_tripupdates (tdata_t *td, uint8_t *buf, size_t len);

void tdata_apply_gtfsrt_tripupdates_file (tdata_t *td, char *filename);

void tdata_clear_gtfsrt (tdata_t *td);

#endif /* RRRR_FEATURE_REALTIME_EXPANDED */

#endif /* _TDATA_REALTIME_H */
