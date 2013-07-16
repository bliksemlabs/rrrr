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

void bitset_clear(BitSet *self, uint32_t index) {
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

/* De-allocate a BitSet struct as well as the memory it references internally for the bit fields. */
void bitset_destroy(BitSet *self) {
    free(self->chunks);
    free(self);
}

/* 
Return the next set index in this BitSet greater than or equal to the specified index.
Returns BITSET_NONE if there are no more set bits. 
*/
inline uint32_t bitset_next_set_bit(BitSet *bs, uint32_t index) {
    uint64_t *chunk = bs->chunks + (index >> 6);
    uint64_t mask = 1ull << (index & 0x3F);
    while (index < bs->capacity) {
        /* check current bit */
        if (mask & *chunk)
            return index;
        /* move to next bit in current chunk */
        mask <<= 1; // no-op on first call because mask is 0
        index += 1; // brings index to 0 on first call
        /* begin a new chunk */
        if (mask == 0) {
            mask = 1ull;
            ++chunk;
            /* spin forward to next chunk containing a set bit, if no set bit was found */
            while ( ! *chunk ) {
                ++chunk;
                index += 64;
                if (index >= bs->capacity)
                    return BITSET_NONE;
            }
        }
    }
    return BITSET_NONE;
}

/*

Enumerating very sparse 50kbit bitset 100k times:
10.592 sec without skipping chunks == 0
0.210 sec with skipping

Enumerating very dense 50kbit bitset 100k times:
20.525 sec without skipping
20.599 sec with skipping

So no real slowdown from skipping checks, but great improvement on sparse sets.
Maybe switching to 32bit ints would allow finer-grained skipping.

Why is the dense set so much slower? Probably function call overhead (1 per result). 
For dense set:
Adding inline keyword and -O2 reduces to 6.1 seconds.
Adding inline keyword and -O3 reduces to 7.6 seconds (note this is higher than -O2).
Adding -O2 without inline keyword reduces to 9.8 seconds.
Adding -O3 without inline keyword reduces to 7.6 seconds.

So enumerating the dense set 8 times takes roughly 0.0005 sec.

For sparse set:
Adding inline keyword and -O2 reduces to 0.064 seconds (0.641 sec for 1M enumerations).

*/

int test_main (void) {
    uint32_t max = 50000;
    BitSet *bs = bitset_new(max);
    for (uint32_t i = 0; i < 50000; i += 2)
        bitset_set(bs, i);
    for (uint32_t i = 0; i < 100000; i++) {
        bitset_enumerate(bs);
    }
    bitset_destroy(bs);
    return 0;
}
