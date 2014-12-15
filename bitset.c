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
#include <assert.h>


/* Initialize a pre-allocated bitset struct,
 * allocating memory for bits_t holding the bits.
 */
static void bitset_init(BitSet *self, uint32_t capacity) {
    self->capacity = capacity;
    /* round upwards */
    self->n_chunks = (capacity + (BS_BITS - 1)) >> BS_SHIFT;
    self->chunks = (bits_t *) calloc(self->n_chunks, sizeof(bits_t));
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
    if (bs != NULL)
        bitset_init(bs, capacity);

    return bs;
}

/* De-allocate a BitSet struct as well as the memory it references
 * internally for the bit fields.
 */
void bitset_destroy(BitSet *self) {
    if (!self) return;

    free(self->chunks);
    free(self);
}

void bitset_clear(BitSet *self) {
    uint32_t i_chunk = self->n_chunks;
    do {
        i_chunk--;
        self->chunks[i_chunk] = (bits_t) 0;
    } while (i_chunk);
}

void bitset_black(BitSet *self) {
    uint32_t i_chunk = self->n_chunks;
    do {
        i_chunk--;
        self->chunks[i_chunk] = ~((bits_t) 0);
    } while (i_chunk);
}

void bitset_mask_and(BitSet *self, BitSet *mask) {
    uint32_t i_chunk = self->n_chunks;

    assert (self->capacity == mask->capacity);

    do {
        i_chunk--;
        self->chunks[i_chunk] &= mask->chunks[i_chunk];
    } while (i_chunk);
}

/* Our bitset code is storing a long number of bits by packing an array of
 * uint128_t's. To find at what location a bit should be read or written, we
 * must firstly figure out at what index the bit is stored.
 * An uint128_t is 128bit long, thus to figure out the index in our array for
 * bit 137 would divide by 128 and the remainder will be the nth place in the
 * at the selected index.
 * Dividing by 128 can be done using a bitshift right 7.
 * Calculating the remainder of 128 can be done using modulo 128 or using
 * 137 & ((1 << 7) - 1), the latter equals 127.
 *
 * To resume:
 * 137 / 128 = 137 >> 7
 * 137 % 128 = 137 & 127
 *
 * Or bitwise:
 * 10001001      = 137
 * 10001001 >> 7 =
 *        1      =   1
 *
 * 10001001      =
 * 01111111      = 127
 *           &   =
 * 00001001      =   9
 */

void bitset_set(BitSet *self, uint32_t index) {
    assert (index < self->capacity);

    self->chunks[index >> BS_SHIFT] |= ((bits_t) 1) << (index & (BS_BITS - 1));
}

void bitset_unset(BitSet *self, uint32_t index) {
    assert (index < self->capacity);

    self->chunks[index >> BS_SHIFT] &= ~(((bits_t) 1) << (index & (BS_BITS - 1)));
}

bool bitset_get(BitSet *self, uint32_t index) {
    assert (index < self->capacity);

    return self->chunks[index >> BS_SHIFT] & ((bits_t) 1) << (index & (BS_BITS - 1));
}


/* Return the next set index in this BitSet greater than or equal to
 * the specified index. Returns BITSET_NONE if there are no more set bits.
 */
uint32_t bitset_next_set_bit(BitSet *bs, uint32_t index) {
    bits_t *chunk = bs->chunks + (index >> BS_SHIFT);
    bits_t mask = ((bits_t) 1) << (index & (BS_BITS - 1));
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
            mask = ((bits_t) 1);
            ++chunk;
            /* spin forward to next chunk containing a set bit,
             * if no set bit was found
             */
            while ( ! *chunk ) {
                ++chunk;
                index += BS_BITS;
                if (index >= bs->capacity) {
                    return BITSET_NONE;
                }
            }
        }
    }
    return BITSET_NONE;
}

#ifdef RRRR_DEBUG

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

#endif /* RRRR_DEBUG */
