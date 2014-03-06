/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* radixtree.h */

#ifndef _RADIXTREE_H
#define _RADIXTREE_H

#include <stdint.h>

// roughly the length of common prefixes in IDs
#define RADIX_TREE_PREFIX_SIZE 4
#define RADIX_TREE_NONE UINT32_MAX

// with prefix size of 4 and -m32, edge size is 16 bytes, total 11.9MB
// with prefix size of 4 and -m64, edge size is 24 bytes, total 17.8MB
// total size of all ids is 15.6 MB
// could use int indexes into a fixed-size, pre-allocated edge pool.

/* Represents both an edge and the node it leads to. A zero-length prefix indicates an empty edge list. A NULL next-pointer indicates the last edge in the list. */
// TODO RENAME rxt_edge
struct edge {
    struct edge *next;  // the next parallel edge out of the same parent node (NULL indicates end of list)
    struct edge *child; // the first edge in the list reached by traversing this edge (consuming its prefix)
    uint32_t value;
    char prefix[RADIX_TREE_PREFIX_SIZE];
};

// TODO probably better to have two different structs for clarity
typedef struct edge RadixTree;

RadixTree *rxt_load_strings_from_file (char *filename);

RadixTree *rxt_load_strings_from_tdata (char *strings, uint32_t width, uint32_t length);

RadixTree *rxt_new ();

void rxt_insert (struct edge *root, const char *key, uint32_t value);

uint32_t rxt_find (struct edge *root, const char *key);

/* Debug functions */

int rxt_edge_count (struct edge *e);

void rxt_edge_print (struct edge *e);

#endif // _RADIXTREE_H
