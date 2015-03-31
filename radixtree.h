/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
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
typedef struct rxt_edge rxt_edge_t;
struct rxt_edge {
    /* the next parallel edge out of the same parent node,
     * NULL indicates end of list
     */
    rxt_edge_t *next;

    /* the first edge in the list reached by traversing this
     * edge (consuming its prefix)
     */
    rxt_edge_t *child;
    uint32_t value;
    char prefix[RRRR_RADIXTREE_PREFIX_SIZE];
};

typedef struct radixtree_s radixtree_t;
struct radixtree_s {
    rxt_edge_t *root;
    void *base;
    size_t size;
};

radixtree_t *radixtree_new (void);

radixtree_t *radixtree_load_strings_from_file (char *filename);

radixtree_t *radixtree_load_strings_from_tdata (char *strings, uint32_t width, uint32_t length);

void radixtree_destroy (radixtree_t *r);

bool radixtree_insert (radixtree_t *r, const char *key, uint32_t value);

rxt_edge_t* radixtree_find (radixtree_t *r, const char *key);
uint32_t radixtree_find_exact (radixtree_t *r, const char *key);
uint32_t radixtree_find_prefix (radixtree_t *r, const char *key, rxt_edge_t *result);

#ifdef RRRR_DEBUG
uint32_t rxt_edge_count (rxt_edge_t *e);

void rxt_edge_print (rxt_edge_t *e);
#endif

#endif /* _RADIXTREE_H */
