/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* bitset.c : compact enumerable bit array */
#include "bitset.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Initialize a pre-allocated bitset struct,
 * allocating memory for the uint64s holding the bits.
 */
static void bitset_init(BitSet *self, uint32_t capacity) {
    self->capacity = capacity;
    /* round upwards */
    self->nchunks = (capacity + 63) / 64;
    self->chunks = (uint64_t *) calloc(self->nchunks, sizeof(uint64_t));
    if (self->chunks == NULL) {
        fprintf(stderr, "bitset chunk allocation failure.");
        exit(1);
    }
}

/* Allocate a new bitset of the specified capacity,
 * and return a pointer to the BitSet struct.
 */
BitSet *bitset_new(uint32_t capacity) {
    BitSet *bs = (BitSet *) malloc(sizeof(BitSet));
    if (bs == NULL) {
        fprintf(stderr, "bitset allocation failure.");
        exit(1);
    }
    bitset_init(bs, capacity);
    return bs;
}

static void index_check(BitSet *self, uint32_t index) {
    if (index >= self->capacity) {
        fprintf(stderr, "bitset index %d out of range [0, %d)\n",
                        index, self->capacity);
        exit(1);
   }
}

void bitset_clear(BitSet *self) {
    memset(self->chunks, 0, sizeof(uint64_t) * self->nchunks);
}

void bitset_set(BitSet *self, uint32_t index) {
    uint64_t bitmask;
    index_check(self, index);
    /* 1ull */
    bitmask = ((uint64_t) 1) << (index % 64);
    self->chunks[index / 64] |= bitmask;
}

void bitset_unset(BitSet *self, uint32_t index) {
    uint64_t bitmask;
    index_check(self, index);
    /* 1ull */
    bitmask = ~(((uint64_t) 1) << (index % 64));
    self->chunks[index / 64] &= bitmask;
}

bool bitset_get(BitSet *self, uint32_t index) {
    uint64_t bitmask;
    index_check(self, index);
    /* need to specify that literal 1 is >= 64 bits wide. */
    /* 1ull */
    bitmask = ((uint64_t) 1) << (index % 64);
    return self->chunks[index / 64] & bitmask;
}

void bitset_dump(BitSet *self) {
    uint32_t i;
    for (i = 0; i < self->capacity; ++i)
        if (bitset_get(self, i))
            fprintf(stderr, "%d ", i);
    fprintf(stderr, "\n\n");
}

uint32_t bitset_enumerate(BitSet *self) {
    uint32_t total = 0;
    uint32_t elem;
    for (elem = bitset_next_set_bit(self, 0);
         elem != BITSET_NONE;
         elem = bitset_next_set_bit(self, elem + 1)) {
        #if 0
        fprintf (stderr, "%d ", elem);
        #endif
        total += elem;
    }
    return total;
}

/* De-allocate a BitSet struct as well as the memory it references
 * internally for the bit fields.
 */
void bitset_destroy(BitSet *self) {
    free(self->chunks);
    free(self);
}

/* Return the next set index in this BitSet greater than or equal to
 * the specified index. Returns BITSET_NONE if there are no more set bits.
 */
uint32_t bitset_next_set_bit(BitSet *bs, uint32_t index) {
    /* 2^6 == 64 */
    uint64_t *chunk = bs->chunks + (index >> 6);
    /* binary 111111, i.e. the six lowest-order bits */
    /* 1ull */
    uint64_t mask = ((uint64_t) 1) << (index & 0x3F);
    while (index < bs->capacity) {
        /* check current bit in current chunk */
        if (mask & *chunk)
            return index;
        /* move to next bit in current chunk */
        mask <<= 1;
        index += 1;
        /* begin a new chunk */
        if (mask == 0) {
            /* 1ull */
            mask = ((uint64_t) 1);
            ++chunk;
            /* spin forward to next chunk containing a set bit,
             * if no set bit was found
             */
            while ( ! *chunk ) {
                ++chunk;
                index += 64;
                if (index >= bs->capacity) {
                    return BITSET_NONE;
                }
            }
        }
    }
    return BITSET_NONE;
}
