/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "rrrr_types.h"

#if RRRR_MAX_BANNED_STOP_POINTS > 0 || RRRR_BAX_BANNED_STOPS_HARD > 0
void set_add_sp (spidx_t *set, uint8_t  *length, uint8_t max_length, spidx_t value);
bool set_in_sp (const spidx_t *set, uint8_t length, spidx_t value);
#endif

#if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
void set_add_jp (jpidx_t *set, uint8_t  *length, uint8_t max_length, jpidx_t value);
#endif

#if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
void set_add_vj (jpidx_t *set1, jp_vjoffset_t *set2, uint8_t  *length, uint8_t max_length, jpidx_t value1, jp_vjoffset_t value2);
bool set_in_vj (const jpidx_t *set1, const jp_vjoffset_t *set2, uint8_t length, jpidx_t value1, jp_vjoffset_t value2);
#endif

#if RRRR_MAX_FILTERED_OPERATORS > 0
void set_add_uint8 (uint8_t *set, uint8_t *length, uint8_t max_length, uint8_t value);
bool set_in_uint8 (const uint8_t *set, uint8_t length, uint8_t value);
#endif
