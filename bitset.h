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

typedef struct bitset_s bitset_t;
struct bitset_s {
    bits_t  *chunks;
    uint32_t n_chunks;
    uint32_t capacity;
};

/* Allocate a new bitset of the specified capacity,
 * and return a pointer to the bitset_t struct.
 */
bitset_t *bitset_new(uint32_t capacity);

/* De-allocate a bitset_t struct as well as the memory it references
 * internally for the bit fields.
 */
void bitset_destroy(bitset_t *self);

/* Perform a logical AND on this bitset_t using the given mask
*/
void bitset_mask_and(bitset_t *self, bitset_t *mask);

/* Set all indices in this bitset_t to true. */
void bitset_black(bitset_t *self);

/* Unset all indices in this bitset_t. */
void bitset_clear(bitset_t *self);

/* Set specified index in this bitset_t. */
void bitset_set(bitset_t *self, uint32_t index);

/* Unset specified index in this bitset_t. */
void bitset_unset(bitset_t *self, uint32_t index);

/* Return whether the specified index is set in this bitset_t */
bool bitset_get(bitset_t *self, uint32_t index);

/* Return the next set index in this bitset_t equal or greater than
 * the specified index. Returns BITSET_NONE if there are no more set bits.
 */
uint32_t bitset_next_set_bit(bitset_t*, uint32_t index);

#ifdef RRRR_DEBUG
/* Print a string-representation of this bitset to STDERR */
void bitset_dump(bitset_t *self);
/* Return an enumeration of all the indices set in the bitset_t */
uint32_t bitset_enumerate(bitset_t *self);
#endif /* RRRR_DEBUG */

#endif /* _BITSET_H */

