/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
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

void linkedlist_init (linkedlist_t *list);
linkedlist_t *linkedlist_new (void);
void linkedlist_destroy (linkedlist_t **list);
void linkedlist_push (linkedlist_t *list, void *payload);
void linkedlist_enqueue (linkedlist_t *list, void *payload);
void *linkedlist_pop (linkedlist_t *list);
