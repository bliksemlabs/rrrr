/* bitset.h */

#ifndef _BITSET_H
#define _BITSET_H

#include <stdint.h>
#include <stdbool.h>

#define BITSET_NONE UINT32_MAX

typedef struct bitset_s BitSet;
struct bitset_s {
    uint32_t capacity;
    uint64_t *chunks;
    uint32_t nchunks;
};

typedef struct bitset_iter_s BitSetIterator;
struct bitset_iter_s {
    uint64_t *chunk;
    uint64_t mask;
    uint32_t index;
    uint32_t capacity;
};

BitSet *bitset_new(uint32_t capacity);

void bitset_reset(BitSet *self); // rename to bitset_clear?

void bitset_set(BitSet *self, uint32_t index);

bool bitset_get(BitSet *self, uint32_t index);

void bitset_dump(BitSet *self);

void bitset_destroy(BitSet *self);

uint32_t bitset_next_set_bit(BitSet*, uint32_t index);

#endif // _BITSET_H

