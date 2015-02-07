#include "rrrr_types.h"
#include "set.h"

void set_add_jp (uint32_t *set,
                 uint8_t  *length, uint8_t max_length,
                 uint32_t value) {
    uint8_t i;

    if (*length >= max_length) return;

    for (i = 0; i < *length; ++i) {
        if (set[i] == value) return;
    }

    set[*length] = value;
    (*length)++;
}

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

void set_add_trip (uint32_t *set1, uint16_t *set2,
                   uint8_t  *length, uint8_t max_length,
                   uint32_t value1, uint16_t value2) {
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

