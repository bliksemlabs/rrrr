/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "radixtree.h"
#include "config.h"

/* All nodes are identical in size, allowing use with no dynamic allocation
 * (in a contiguous block of memory). Only supports insertion and retrieval,
 * not deleting.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

static struct rxt_edge *rxt_edge_new () {
    struct rxt_edge *e = (struct rxt_edge *) malloc(sizeof(struct rxt_edge));
    if (e == NULL) return NULL;

    e->next = NULL;
    e->child = NULL;
    e->value = RADIXTREE_NONE;
    e->prefix[0] = '\0';
    return e;
}

static void rxt_init(radixtree_t *self) {
    self->root = rxt_edge_new();
    self->base = NULL;
    self->size = 0;
}

radixtree_t *radixtree_new () {
    radixtree_t *r = (radixtree_t *) malloc(sizeof(radixtree_t));
    if (r == NULL) return NULL;

    rxt_init(r);

    if (r->root == NULL) {
        free(r);
        return NULL;
    }

    return r;
}

/* Insert a string-to-int mapping into the prefix tree.
 * Uses tail recursion, not actual recursive function calls.
 */
bool radixtree_insert (radixtree_t *r, const char *key, uint32_t value) {
    const char *k = key;
    struct rxt_edge *e = r->root;

    /* Loop over edges labeled to continuation from within nested loops. */
    tail_recurse: while (e != NULL) {
        char *p;
        if (*k == '\0') {
            fprintf (stderr, "Attempt to insert 0-length string.\n");
            return false; /* refuse to insert 0-length strings. */
        }
        p = e->prefix;
        if (*p == '\0') {
            /* Case 1: We have key characters and a fresh (empty) edge for
             * use (whose next pointer should also be NULL).
             * This section is hit whenever we have an empty tree, add a new
             * edge to an edge list, or add a new level to the tree.
             */
            uint32_t i;
            for (i = 0; i < RRRR_RADIXTREE_PREFIX_SIZE; ++i, ++k, ++p) {
                /* copy up to RRRR_RADIXTREE_PREFIX_SIZE characters into this
                 * edge's prefix
                 */
                *p = *k;
                if (*k == '\0') break;
                /* note we have copied the 0 byte to indicate the prefix is
                 * shorter than RRRR_RADIXTREE_PREFIX_SIZE
                 */
            }
            if (*k == '\0') {
                /* This edge consumes all characters in the key, save the
                 * value here. Catches both the case where we have consumed
                 * all RRRR_RADIXTREE_PREFIX_SIZE characters and the one where we
                 * have consumed less.
                 */
                e->value = value;
                return true;
            } else {
                /* Some characters remain in the key, make an empty child edge
                 * list and tail-recurse.
                 */
                e->child = rxt_edge_new();
                if (e->child == NULL) return false; /* allocation failed */

                e = e->child;
                goto tail_recurse;
            }
        }
        if (*k == *p) {
            /* Case 2: This edge matches the key at least partially,
             * consume some characters.
             */
            uint32_t i;
            for (i = 0; i < RRRR_RADIXTREE_PREFIX_SIZE; ++i, ++k, ++p) {
                if (*p == '\0') break;
                /* whole prefix was consumed */

                if (*k != *p) {
                    /* Key is not in tree but partially matches an edge. Must
                     * split the edge. Might be nice to somehow pull this out
                     * of the for loop, to avoid goto. Then again purpose of
                     * goto is clear.
                     */
                    struct rxt_edge *new;
                    uint32_t j;
                    char *n, *o;

                    new = rxt_edge_new();
                    if (new == NULL) return false; /* allocation failed */

                    new->value = e->value;
                    new->child = e->child;
                    e->value = RADIXTREE_NONE;
                    e->child = new;
                    /* copy the rest of e's prefix into the new child edge */
                    n = new->prefix;
                    o = p;
                    for (j = i; j < RRRR_RADIXTREE_PREFIX_SIZE && *o != '\0'; ++j) {
                        *(n++) = *(o++);
                    }
                    *n = '\0';
                    /* Child string is known to be less than
                     * RRRR_RADIXTREE_PREFIX_SIZE in length.
                     */
                    *p = '\0';
                    /* Truncate old prefix at the point it was split. */
                    if (*k == '\0') {
                        /* No characters remain in the key.
                         * No need to recurse.
                         */
                        e->value = value;
                        return true;
                    }
                    e = new;
                    /* Tail-recurse using new edge list to consume
                     * remaining characters
                     */
                    goto tail_recurse;
                    /* A simple 'continue' does not suffice, as this is a
                     * nested loop.
                     */
                }
            }
            /* We have consumed the entire prefix, either with or
             * without hitting a terminator byte.
             */
            if (*k == '\0') {
                /* This edge consumed the entire key, it is a match.
                 * Replace its value.
                 */
                e->value = value;
                return true;
            }
            /* Key characters remain, tail-recurse on those remaining
             * characters, creating a new edge list as needed.
             */
            if (e->child == NULL) e->child = rxt_edge_new();
            if (e->child == NULL) return false; /* allocation failed */
            e = e->child;
            goto tail_recurse;
        }
        /* Case 3: No edges so far have been empty or matched at all.
         * Move on to the next edge in the edge list.
         */
        if (e->next == NULL) {
            /* We have remaining characters in they key, no edges match,
             * but no next edge. Make an empty edge to use.
             */
            e->next = rxt_edge_new();
            if (e->next == NULL) return false; /* allocation failed */

            /* Note this edge will have *prefix == '\0' so will
             * be used to consume characters.
             */
        }
        e = e->next;
        /* Move on to the next edge in the list on the same tree level. */
    }
    return false;
    /* should never happen unless allocation fails, giving a NULL next edge. */
}

uint32_t radixtree_find (radixtree_t *r, const char *key) {
    const char *k = key;
    struct rxt_edge *e = r->root;
    while (e != NULL) {
        const char *p = e->prefix;
        if (*k == *p) { /* we have a match, consume some characters */
            uint32_t i;
            for (i = 0; i < RRRR_RADIXTREE_PREFIX_SIZE; ++i, ++k, ++p) {
                if (*p == '\0') break;
                /* prefix char is 0, reached the end of this edge's
                 * prefix string.
                 */
                if (*k != *p) return RADIXTREE_NONE;
                /* key was not in tree */
            }
            /* We have consumed the prefix with or without
             * hitting a terminator byte.
             */
            if (*k == '\0') return e->value;
            /* This edge consumed the entire key. */
            e = e->child;
            /* Key characters remain, tail-recurse on those
             * remaining characters. Child might be NULL.
             */
            continue;
        }
        e = e->next;
        /* Next edge in the list on the same tree level */
    }
    return RADIXTREE_NONE;
    /* Ran out of edges to traverse, no match was found. */
}

radixtree_t *radixtree_load_strings_from_file (char *filename) {
    radixtree_t *r;
    char *strings_end, *s;
    struct stat st;
    uint32_t idx;
    int fd;

    r = radixtree_new();
    if (r == NULL) return NULL;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "The input file %s could not be found.\n", filename);
        goto fail_free_r;
    }

    if (stat(filename, &st) == -1) {
        fprintf(stderr, "The input file %s could not be stat.\n", filename);
        goto fail_close_fd;
    }

    r->size = st.st_size;

    #if defined(RRRR_TDATA_IO_MMAP)
    r->base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (r->base == MAP_FAILED) {
        fprintf(stderr, "The input file %s could not be mapped.\n", filename);
        goto fail_close_fd;
    }
    #else
    r->base = malloc(st.st_size + 1);
    if (r->base == NULL) {
        fprintf(stderr, "Could not allocate memomry to store %s.\n", filename);
        goto fail_close_fd;
    } else {
        ssize_t len = read (fd, r->base, r->size);
        ((char *) r->base)[len] = '\0';

        if (len != (ssize_t) r->size)  {
            free (r->base);
            r->base = NULL;
            goto fail_close_fd;
        }
    }
    #endif

    strings_end = (char *) r->base + r->size;
    s = (char *) r->base;
    idx = 0;

    #ifdef RRRR_DEBUG
    fprintf (stderr, "Indexing strings...\n");
    #endif
    while (s < strings_end) {
        radixtree_insert (r, s, idx);
        while(*(s++) != '\0') { }
        idx += 1;
    }

    /* We must close the file descriptor otherwise we will
     * leak it. Because mmap has created a reference to it
     * there will not be a problem.
     */
    close (fd);

    #if 0
    rxt_compress (root);
    fprintf (stderr, "total number of edges: %d\n", edge_count(root));
    fprintf (stderr, "size of one edge: %ld\n", sizeof(struct rxt_edge));
    fprintf (stderr, "total size of all edges: %ld\n",
                     edge_count(root) * sizeof(struct rxt_edge));
    #endif

    return r;

fail_close_fd:
    close(fd);

fail_free_r:
    free(r);

    return NULL;
}

static void rxt_edge_free (struct rxt_edge *e) {
    if (e == NULL) return;

    rxt_edge_free (e->next);
    rxt_edge_free (e->child);

    free(e);
}

void radixtree_destroy (radixtree_t *r) {
    if (r == NULL) return;

    rxt_edge_free (r->root);

    #if defined(RRRR_TDATA_IO_MMAP)
    if (r->base) munmap(r->base, r->size);
    #else
    if (r->base) free (r->base);
    #endif

    free(r);
}

#ifdef RRRR_DEBUG
static uint32_t edge_prefix_length (struct rxt_edge *e) {
    uint32_t n = 0;
    char *c = e->prefix;
    while (*c != '\0' && n < RRRR_RADIXTREE_PREFIX_SIZE) {
        ++n;
        ++c;
    }
    return n;
}

uint32_t rxt_edge_count (struct rxt_edge *e) {
    uint32_t n = 0;
    if (e != NULL) {
        n += 1;
        n += rxt_edge_count(e->child);
        n += rxt_edge_count(e->next);
    }
    return n;
}

void rxt_edge_print (struct rxt_edge *e) {
    if (e == NULL) return;
    fprintf (stderr, "\nedge [%p]\n", (void *) e);
    /* variable width string format character */
    fprintf (stderr, "prefix '%.*s'\n", RRRR_RADIXTREE_PREFIX_SIZE, e->prefix);
    fprintf (stderr, "length %d\n", edge_prefix_length(e));
    fprintf (stderr, "value  %d\n", e->value);
    fprintf (stderr, "next   %p\n", (void *) e->next);
    fprintf (stderr, "child  %p\n", (void *) e->child);
    rxt_edge_print(e->next);
    rxt_edge_print(e->child);
}
#endif

#if 0
/* TODO: returns a compacted copy of the tree */
struct node *compact (struct rxt_edge *root) {
    return NULL;
}

/* Compresses paths in place. Not well tested,
 * and only seems to remove a few edges from 600k when using numbers.
 */
void rxt_compress (struct rxt_edge *root) {
    struct rxt_edge *e = root;
    if (e == NULL) return;
    while (e->child != NULL &&
           e->child->next == NULL && e->value == RADIXTREE_NONE) {
        uint32_t l0 = edge_prefix_length(e);
        uint32_t l1 = edge_prefix_length(e->child);
        if (l0 + l1 <= RRRR_RADIXTREE_PREFIX_SIZE) {
            struct rxt_edge *new_child;
            uint32_t i;
            char *c0, *c1;

            fprintf (stderr, "compressing %.*s and %.*s.\n",
                             RRRR_RADIXTREE_PREFIX_SIZE, e->prefix,
                             RRRR_RADIXTREE_PREFIX_SIZE, e->child->prefix);
            c0 = e->prefix + l0;
            c1 = e->child->prefix;

            for (i = 0; i < l1; ++i) *(c0++) = *(c1++);

            if (l0 + l1 < RRRR_RADIXTREE_PREFIX_SIZE) *c0 = '\0';

            e->value = e->child->value;
            new_child = e->child->child;
            free (e->child);
            e->child = new_child;
        }
    }
    rxt_compress (e->child);
    rxt_compress (e->next);
}
#endif
