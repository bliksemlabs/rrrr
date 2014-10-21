/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _TDATA_VALIDATION_H
#define _TDATA_VALIDATION_H

#include "tdata.h"

int tdata_validation_boarding_alighting(tdata_t *tdata);
int tdata_validation_coordinates(tdata_t *tdata);
int tdata_validation_increasing_times(tdata_t *tdata);
int tdata_validation_symmetric_transfers(tdata_t *tdata);
bool tdata_validation_check_coherent (tdata_t *tdata);

#endif /* _TDATA_VALIDATION_H */
