/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* linkedlist.h */

#include <stdint.h>
#include <stdlib.h>

typedef struct list_node {
    struct list_node *next;
    void *payload;
} listnode_t;

typedef struct {
    listnode_t *head;
    listnode_t *tail;
    uint32_t size;
} linkedlist_t;
