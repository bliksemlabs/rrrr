/* bitset.c : compact enumerable bit array */
#include "bitset.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

inline void bitset_reset(BitSet *self) {
    memset(self->chunks, 0, sizeof(uint64_t) * self->nchunks);
}

/* Initialize a pre-allocated bitset struct, allocating memory for the uint64s holding the bits. */
static void bitset_init(BitSet *self, uint32_t capacity) {
    self->capacity = capacity;
    self->nchunks = capacity / 64;
    if (capacity % 64) 
        self->nchunks += 1;
    self->chunks = malloc(sizeof(uint64_t) * self->nchunks);
    if (self->chunks == NULL) {
        printf("bitset chunk allocation failure.");
        exit(1);
    }
    bitset_reset(self);
}

/* Allocate a new bitset of the specified capacity, and return a pointer to the BitSet struct. */
BitSet *bitset_new(uint32_t capacity) {
    BitSet *bs = malloc(sizeof(BitSet));
    if (bs == NULL) {
        printf("bitset allocation failure.");
        exit(1);
    }
    bitset_init(bs, capacity);
    return bs;
}

static inline void index_check(BitSet *self, uint32_t index) {
    if (index >= self->capacity) {
        printf("bitset index %d out of range [0, %d)\n", index, self->capacity);
        exit(1);    
   }
}

void bitset_set(BitSet *self, uint32_t index) {
    index_check(self, index);
    uint64_t bitmask = 1ull << (index % 64);
    self->chunks[index / 64] |= bitmask;
}

void bitset_unset(BitSet *self, uint32_t index) {
    index_check(self, index);
    uint64_t bitmask = ~(1ull << (index % 64));
    self->chunks[index / 64] &= bitmask;
}

bool bitset_get(BitSet *self, uint32_t index) {
    index_check(self, index);
    uint64_t bitmask = 1ull << (index % 64); // need to specify that literal 1 is >= 64 bits wide.
    return self->chunks[index / 64] & bitmask;
}

void bitset_dump(BitSet *self) {
    for (uint32_t i = 0; i < self->capacity; ++i)
        if (bitset_get(self, i))
            printf("%d ", i);
    printf("\n\n");
}

uint32_t bitset_enumerate(BitSet *self) {
    uint32_t total = 0;
    for (uint32_t elem = bitset_next_set_bit(self, 0); 
                  elem != BITSET_NONE; 
                  elem = bitset_next_set_bit(self, elem + 1)) {
        //printf ("%d ", elem); 
        total += elem;
    }
    return total;
}

/* 
  De-allocate a BitSet struct as well as the memory it references internally for the bit fields. 
*/
void bitset_destroy(BitSet *self) {
    free(self->chunks);
    free(self);
}

/* 
  Return the next set index in this BitSet greater than or equal to the specified index.
  Returns BITSET_NONE if there are no more set bits. 
*/
inline uint32_t bitset_next_set_bit(BitSet *bs, uint32_t index) {
    uint64_t *chunk = bs->chunks + (index >> 6);  // 2^6 == 64
    uint64_t mask = 1ull << (index & 0x3F);       // binary 111111, i.e. the six lowest-order bits 
    while (index < bs->capacity) {
        /* check current bit in current chunk */
        if (mask & *chunk)
            return index;
        /* move to next bit in current chunk */
        mask <<= 1;
        index += 1;
        /* begin a new chunk */
        if (mask == 0) {
            mask = 1ull;
            ++chunk;
            /* spin forward to next chunk containing a set bit, if no set bit was found */
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


