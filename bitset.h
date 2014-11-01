/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* bitset.h */

#ifndef _BITSET_H
#define _BITSET_H

#include <stdint.h>
#include <stdbool.h>

#define BITSET_NONE UINT32_MAX

#if defined ( RRRR_BITSET_128 )
typedef unsigned __int128 bits_t;
#define BS_BITS  128
#define BS_SHIFT 7

#elif defined ( RRRR_BITSET_64 )
typedef uint64_t bits_t;
#define BS_BITS  64
#define BS_SHIFT 6

#else
typedef uint32_t bits_t;
#define BS_BITS  32
#define BS_SHIFT 5
#endif

typedef struct bitset_s BitSet;
struct bitset_s {
    bits_t  *chunks;
    uint32_t nchunks;
    uint32_t capacity;
};

BitSet *bitset_new(uint32_t capacity);

void bitset_destroy(BitSet *self);

void bitset_mask_and(BitSet *self, BitSet *mask);

void bitset_black(BitSet *self);

void bitset_clear(BitSet *self);

void bitset_set(BitSet *self, uint32_t index);

void bitset_unset(BitSet *self, uint32_t index);

bool bitset_get(BitSet *self, uint32_t index);

uint32_t bitset_next_set_bit(BitSet*, uint32_t index);

void bitset_dump(BitSet *self);

uint32_t bitset_enumerate(BitSet *self);

#endif /* _BITSET_H */

