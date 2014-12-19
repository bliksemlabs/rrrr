/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _RADIXTREE_H
#define _RADIXTREE_H

#include "config.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RADIXTREE_NONE UINT32_MAX

/* Represents both an edge and the node it leads to. A zero-length prefix
 * indicates an empty edge list. A NULL next-pointer indicates the last edge
 * in the list.
 */
struct rxt_edge {
    /* the next parallel edge out of the same parent node,
     * NULL indicates end of list
     */
    struct rxt_edge *next;

    /* the first edge in the list reached by traversing this
     * edge (consuming its prefix)
     */
    struct rxt_edge *child;
    uint32_t value;
    char prefix[RRRR_RADIXTREE_PREFIX_SIZE];
};

typedef struct radixtree_s radixtree_t;
struct radixtree_s {
    struct rxt_edge *root;
    void *base;
    size_t size;
};

radixtree_t *radixtree_new ();

radixtree_t *radixtree_load_strings_from_file (char *filename);

radixtree_t *radixtree_load_strings_from_tdata (char *strings, uint32_t width, uint32_t length);

void radixtree_destroy (radixtree_t *r);

bool radixtree_insert (radixtree_t *r, const char *key, uint32_t value);

uint32_t radixtree_find (radixtree_t *r, const char *key);

#ifdef RRRR_DEBUG
uint32_t radixtree_edge_count (struct rxt_edge *e);

void radixtree_edge_print (struct rxt_edge *e);
#endif

#endif /* _RADIXTREE_H */
