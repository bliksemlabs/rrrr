#include "config.h"
#include "rrrr_types.h"
#include "set.h"

#if RRRR_MAX_BANNED_STOP_POINTS > 0 || RRRR_BAX_BANNED_STOPS_HARD > 0
void set_add_sp (spidx_t *set,
                 uint8_t  *length, uint8_t max_length,
                 spidx_t value) {
    uint8_t i;

    if (*length >= max_length) return;

    for (i = 0; i < *length; ++i) {
        if (set[i] == value) return;
    }

    set[*length] = value;
    (*length)++;
}

bool set_in_sp (spidx_t *set, uint8_t length, spidx_t value) {
    uint8_t i = length;
    if (i == 0) return false;
    do {
        i--;
        if (set[i] == value) return true;
    } while (i);
    return false;
}
#endif

#if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
void set_add_jp (jpidx_t *set,
                 uint8_t  *length, uint8_t max_length,
                 jpidx_t value) {
    uint8_t i;

    if (*length >= max_length) return;

    for (i = 0; i < *length; ++i) {
        if (set[i] == value) return;
    }

    set[*length] = value;
    (*length)++;
}

void set_add_vj (jpidx_t *set1, jp_vjoffset_t *set2,
                 uint8_t  *length, uint8_t max_length,
                 jpidx_t value1, jp_vjoffset_t value2) {
    uint8_t i;

    if (*length >= max_length) return;

    for (i = 0; i < *length; ++i) {
        if (set1[i] == value1 &&
            set2[i] == value2) return;
    }

    set1[*length] = value1;
    set2[*length] = value2;
    (*length)++;
}

bool set_in_vj (jpidx_t *set1, jp_vjoffset_t *set2, uint8_t length,
                jpidx_t value1, jp_vjoffset_t value2) {
    uint8_t i = length;
    if (i == 0) return false;
    do {
        i--;
        if (set1[i] == value1 &&
            set2[i] == value2) return true;
    } while (i);
    return false;
}
#endif


