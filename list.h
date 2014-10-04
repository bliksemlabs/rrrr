/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* list.h */

#include <stdint.h>
#include <stdlib.h>

typedef struct list_node {
    struct list_node *next;
    void *payload;
} ListNode;

typedef struct {
    ListNode *head;
    ListNode *tail;
    uint32_t size;
} LinkedList;
