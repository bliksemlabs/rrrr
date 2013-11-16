/* intset.c : a set of integers using a hashtable. only does dynamic allocation on collisions. */

#include "intset.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define INTSET_NONE UINT32_MAX

struct intset {
    int size;
    struct element *elements;
};

struct element {
    uint32_t key;
    struct element *next;
};

static void free_list (struct element *e) {
    while (e != NULL) {
        struct element *next_e = e->next;
        free (e);
        e = next_e;
    }
}

void IntSet_clear (IntSet *is) {
    for (int i = 0; i < is->size; ++i) {
        if (is->elements[i].next != NULL) free_list (is->elements[i].next);
        is->elements[i].key = INTSET_NONE;
        is->elements[i].next = NULL;
    }
}

IntSet *IntSet_new (int n) {
    IntSet *is = malloc (sizeof (IntSet));
    is->size = n;
    is->elements = malloc (sizeof (struct element) * n);
    IntSet_clear (is);
    return is;
}

void IntSet_destroy (IntSet **is) {
    IntSet_clear (*is);
    free ((*is)->elements);
    free (*is);
    *is = NULL;
}

void IntSet_print (IntSet *is) {
    for (int i = 0; i < is->size; ++i) {
        printf ("[%02d] ", i);
        struct element *e = &(is->elements[i]);
        while (e != NULL) {
            if (e->key == INTSET_NONE) printf ("NONE ");
            else printf ("%d ", e->key);
            e = e->next;
        }
        printf ("\n");
    }
}

bool IntSet_contains (IntSet *is, uint32_t value) {
    uint32_t hash = value % is->size;
    struct element *e = &(is->elements[hash]);
    while (e != NULL) {
        if (e->key == value) return true;
        e = e->next;
    }
    return false;    
}

void IntSet_add (IntSet *is, uint32_t value) {
    uint32_t hash = value % is->size;
    struct element *e = &(is->elements[hash]);
    if (e->key != INTSET_NONE) {
        while (true) {
            if (e->key == value) return; // key already in set
            if (e->next == NULL) break;
            e = e->next;
        }
        e->next = malloc (sizeof (struct element));
        e = e->next;
    }
    e->key = value;
    e->next = NULL;
}

int main () {
    IntSet *is = IntSet_new (71);
    for (int i = 0; i < 100; ++i) IntSet_add (is, i * 3);
    for (int i = 0; i < 100; ++i) IntSet_add (is, i * 2);
    for (int i = 0; i < 500; ++i) {
        printf ("%d %s\n", i, IntSet_contains (is, i) ? "YES" : "NO");
    }
    IntSet_print (is);
    IntSet_destroy (&is);
}
