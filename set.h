#include "config.h"
#include "rrrr_types.h"

#if RRRR_MAX_BANNED_STOP_POINTS > 0 || RRRR_BAX_BANNED_STOPS_HARD > 0
void set_add_sp (spidx_t *set, uint8_t  *length, uint8_t max_length, spidx_t value);
bool set_in_sp (spidx_t *set, uint8_t length, spidx_t value);
#endif

#if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
void set_add_jp (jpidx_t *set, uint8_t  *length, uint8_t max_length, jpidx_t value);
void set_add_vj (jpidx_t *set1, jp_vjoffset_t *set2, uint8_t  *length, uint8_t max_length, jpidx_t value1, jp_vjoffset_t value2);
bool set_in_vj (jpidx_t *set1, jp_vjoffset_t *set2, uint8_t length, jpidx_t value1, jp_vjoffset_t value2);
#endif
