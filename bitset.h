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

/* Allocate a new bitset of the specified capacity,
 * and return a pointer to the BitSet struct.
 */
BitSet *bitset_new(uint32_t capacity);

/* De-allocate a BitSet struct as well as the memory it references
 * internally for the bit fields.
 */
void bitset_destroy(BitSet *self);

/* Perform a logical AND on this BitSet using the given mask
*/
void bitset_mask_and(BitSet *self, BitSet *mask);

/* Set all indices in this BitSet to true. */
void bitset_black(BitSet *self);

/* Unset all indices in this BitSet. */
void bitset_clear(BitSet *self);

/* Set specified index in this BitSet. */
void bitset_set(BitSet *self, uint32_t index);

/* Unset specified index in this BitSet. */
void bitset_unset(BitSet *self, uint32_t index);

/* Return whether the specified index is set in this BitSet */
bool bitset_get(BitSet *self, uint32_t index);

/* Return the next set index in this BitSet equal or greater than
 * the specified index. Returns BITSET_NONE if there are no more set bits.
 */
uint32_t bitset_next_set_bit(BitSet*, uint32_t index);

/* Print a string-representation of this bitset to STDERR */
void bitset_dump(BitSet *self);
/* Return an enumeration of all the indices set in the BitSet */
uint32_t bitset_enumerate(BitSet *self);

#endif /* _BITSET_H */

