/* bitset.h */

#ifndef _BITSET_H
#define _BITSET_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bitset_s BitSet;
struct bitset_s {
    int capacity;
    uint64_t *chunks;
    int nchunks;
};

typedef struct bitset_iter_s BitSetIterator;
struct bitset_iter_s {
    uint64_t *chunk;
    uint64_t mask;
    int index;
    int capacity;
};

BitSet *bitset_new(int capacity);

void bitset_reset(BitSet *self);

void bitset_init(BitSet *self, int capacity);

void bitset_set(BitSet *self, int index);

bool bitset_get(BitSet *self, int index);

void bitset_dump(BitSet *self);

void bitset_destroy(BitSet *self);

void bitset_iter_begin(BitSetIterator*, BitSet*);

int bitset_iter_next(BitSetIterator *self);

int bitset_next_set_bit(BitSet*, int index);

#endif // _BITSET_H

