/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* linkedlist.c */

#include "linkedlist.h"

#include <stdint.h>
#include <stdlib.h>

void linkedlist_init (linkedlist_t *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

linkedlist_t *linkedlist_new () {
    linkedlist_t *list = malloc (sizeof(linkedlist_t));
    linkedlist_init (list);
    return list;
}

void linkedlist_destroy (linkedlist_t **list) {
    free (*list);
    *list = NULL;
}

/* Add to head */
void linkedlist_push (linkedlist_t *list, void *payload) {
    listnode_t *node = malloc (sizeof(listnode_t));
    node->payload = payload;
    node->next = list->head;
    list->head = node;
    if (list->tail == NULL) list->tail = node;
    list->size += 1;
}

/* Add to tail */
void linkedlist_enqueue (linkedlist_t *list, void *payload) {
    listnode_t *node = malloc (sizeof(listnode_t));
    node->payload = payload;
    node->next = NULL;
    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->size += 1;
}

/* Remove from head of list */
void *linkedlist_pop (linkedlist_t *list) {
    if (list->head != NULL) {
        void *payload = list->head->payload;
        listnode_t *old_head = list->head;
        list->head = list->head->next;
        free (old_head);
        list->size -= 1;
        return payload;
    }

    return NULL;
}

