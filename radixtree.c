/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "radixtree.h"

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

static void die (const char *msg) {
    fprintf (stderr, "%s\n", msg);
    exit(1);
}

static struct rxt_edge *rxt_edge_new () {
    struct rxt_edge *e = (struct rxt_edge *) malloc(sizeof(struct rxt_edge));
    if (e == NULL) return e;

    e->next = NULL;
    e->child = NULL;
    e->value = RADIX_TREE_NONE;
    e->prefix[0] = '\0';
    return e;
}

RadixTree *rxt_new () {
    return rxt_edge_new();
}

static uint32_t edge_prefix_length (struct rxt_edge *e) {
    uint32_t n = 0;
    char *c = e->prefix;
    while (*c != '\0' && n < RADIX_TREE_PREFIX_SIZE) {
        ++n;
        ++c;
    }
    return n;
}

/* Insert a string-to-int mapping into the prefix tree.
 * Uses tail recursion, not actual recursive function calls.
 */
bool rxt_insert (struct rxt_edge *root, const char *key, uint32_t value) {
    const char *k = key;
    struct rxt_edge *e = root;

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
            for (i = 0; i < RADIX_TREE_PREFIX_SIZE; ++i, ++k, ++p) {
                /* copy up to RADIX_TREE_PREFIX_SIZE characters into this
                 * edge's prefix
                 */
                *p = *k;
                if (*k == '\0') break;
                /* note we have copied the 0 byte to indicate the prefix is
                 * shorter than RADIX_TREE_PREFIX_SIZE
                 */
            }
            if (*k == '\0') {
                /* This edge consumes all characters in the key, save the
                 * value here. Catches both the case where we have consumed
                 * all RADIX_TREE_PREFIX_SIZE characters and the one where we
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
            for (i = 0; i < RADIX_TREE_PREFIX_SIZE; ++i, ++k, ++p) {
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
                    e->value = RADIX_TREE_NONE;
                    e->child = new;
                    /* copy the rest of e's prefix into the new child edge */
                    n = new->prefix;
                    o = p;
                    for (j = i; j < RADIX_TREE_PREFIX_SIZE && *o != '\0'; ++j) {
                        *(n++) = *(o++);
                    }
                    *n = '\0';
                    /* Child string is known to be less than
                     * RADIX_TREE_PREFIX_SIZE in length.
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

uint32_t rxt_find (struct rxt_edge *root, const char *key) {
    const char *k = key;
    struct rxt_edge *e = root;
    while (e != NULL) {
        const char *p = e->prefix;
        if (*k == *p) { /* we have a match, consume some characters */
            uint32_t i;
            for (i = 0; i < RADIX_TREE_PREFIX_SIZE; ++i, ++k, ++p) {
                if (*p == '\0') break;
                /* prefix char is 0, reached the end of this edge's
                 * prefix string.
                 */
                if (*k != *p) return RADIX_TREE_NONE;
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
    return RADIX_TREE_NONE;
    /* Ran out of edges to traverse, no match was found. */
}

#if 0
/* TODO: returns a compacted copy of the tree */
struct node *compact (struct rxt_edge *root) {
    return NULL;
}
#endif

/* Compresses paths in place. Not well tested,
 * and only seems to remove a few edges from 600k when using numbers.
 */
void rxt_compress (struct rxt_edge *root) {
    struct rxt_edge *e = root;
    if (e == NULL) return;
    while (e->child != NULL &&
           e->child->next == NULL && e->value == RADIX_TREE_NONE) {
        uint32_t l0 = edge_prefix_length(e);
        uint32_t l1 = edge_prefix_length(e->child);
        if (l0 + l1 <= RADIX_TREE_PREFIX_SIZE) {
            struct rxt_edge *new_child;
            uint32_t i;
            char *c0, *c1;

            fprintf (stderr, "compressing %.*s and %.*s.\n",
                             RADIX_TREE_PREFIX_SIZE, e->prefix,
                             RADIX_TREE_PREFIX_SIZE, e->child->prefix);
            c0 = e->prefix + l0;
            c1 = e->child->prefix;

            for (i = 0; i < l1; ++i) *(c0++) = *(c1++);

            if (l0 + l1 < RADIX_TREE_PREFIX_SIZE) *c0 = '\0';

            e->value = e->child->value;
            new_child = e->child->child;
            free (e->child);
            e->child = new_child;
        }
    }
    rxt_compress (e->child);
    rxt_compress (e->next);
}

RadixTree *rxt_load_strings_from_file (char *filename) {
    RadixTree *root = NULL;
    struct stat st;
    int fd = open(filename, O_RDONLY);
    uint32_t idx;
    char *strings, *strings_end, *s;

    fd = open(filename, O_RDONLY);
    if (fd == -1) die("could not find input file.");

    if (stat(filename, &st) == -1) {
        die("could not stat input file.");
        goto clean_fd;
    }

    strings = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (strings == MAP_FAILED) {
        die("Could not map radixtree input file.\n");
        goto clean_fd;
    }

    strings_end = strings + st.st_size;
    s = strings;
    idx = 0;
    root = rxt_new ();
    fprintf (stderr, "Indexing strings...\n");
    while (s < strings_end) {
        rxt_insert (root, s, idx);
        while(*(s++) != '\0') { }
        idx += 1;
    }

clean_fd:
    close(fd);
    /*
    rxt_compress (root);
    fprintf (stderr, "total number of edges: %d\n", edge_count(root));
    fprintf (stderr, "size of one edge: %ld\n", sizeof(struct rxt_edge));
    fprintf (stderr, "total size of all edges: %ld\n",
                     edge_count(root) * sizeof(struct rxt_edge));
    */
    return root;
}

RadixTree *rxt_load_strings_from_tdata (char *strings, uint32_t width, uint32_t length) {
    RadixTree *root = rxt_new ();
    char *strings_end = strings + (width * length);
    char *s = strings;
    uint32_t idx = 0;
    #ifdef RRRR_DEBUG
    fprintf (stderr, "Indexing strings...\n");
    #endif
    while (s < strings_end) {
        rxt_insert (root, s, idx);
        s += width;
        idx += 1;
    }

    #if 0
    rxt_compress (root);
    fprintf (stderr, "total number of edges: %d\n", edge_count(root));
    fprintf (stderr, "size of one edge: %ld\n", sizeof(struct rxt_edge));
    fprintf (stderr, "total size of all edges: %ld\n",
                     edge_count(root) * sizeof(struct rxt_edge));
    #endif
    return root;
}

static void rxt_edge_free (struct rxt_edge *e) {
    if (e == NULL) return;

    rxt_edge_free (e->next);
    rxt_edge_free (e->child);

    free(e);
}

void rxt_destroy (RadixTree *root) {
    rxt_edge_free (root);
}

#if RRRR_DEBUG
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
    fprintf (stderr, "prefix '%.*s'\n", RADIX_TREE_PREFIX_SIZE, e->prefix);
    fprintf (stderr, "length %d\n", edge_prefix_length(e));
    fprintf (stderr, "value  %d\n", e->value);
    fprintf (stderr, "next   %p\n", (void *) e->next);
    fprintf (stderr, "child  %p\n", (void *) e->child);
    rxt_edge_print(e->next);
    rxt_edge_print(e->child);
}
#endif
