/* dedup.c : deduplicates strings using a hash table that maps them to integer IDs. */
#include "dedup.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct entry Entry;
struct entry {
    char *key;
    uint32_t val;
    Entry *next;
};

#define SIZE 9973 // prime
static Entry entries[SIZE];
static uint32_t n;
static ProtobufCBinaryData *inverse = NULL; /* Inverse mapping, from ints to strings. */
OSMPBF__StringTable string_table;

/* Copy string pointers over to a dynamically allocated array of ProtobufBinaryData. */
OSMPBF__StringTable *Dedup_string_table () {
    osmpbf__string_table__init(&string_table);
    if (inverse != NULL) free (inverse);
    inverse = malloc(n * sizeof(ProtobufCBinaryData));
    if (inverse == NULL) exit (-1);
    for (int ei = 0; ei < SIZE; ++ei) {
        Entry *e = &(entries[ei]);
        if (e->key != NULL) {
            for (; e != NULL; e = e->next) {
                inverse[e->val].data = (uint8_t*) e->key;
                inverse[e->val].len = strlen(e->key);
            }
        }
    }
    string_table.n_s = n;
    string_table.s = inverse;
    return &string_table; // return pointer to static var
}

static void free_list(Entry *e) {
    while (e != NULL) {
        Entry *next_e = e->next;
        free (e);
        e = next_e;
    }
}

void Dedup_clear() {
    for (int i = 0; i < SIZE; ++i) {
        if (entries[i].next != NULL) free_list (entries[i].next);
        entries[i].key = NULL;
        entries[i].next = NULL;
    }
    if (inverse != NULL) {
        free (inverse);
        inverse = NULL;
    }
    n = 0;
}

void Dedup_init() {
    Dedup_clear();
}

void Dedup_print() {
    for (int i = 0; i < SIZE; ++i) {
        Entry *e = &(entries[i]);
        if (e->key != NULL) {
            printf ("[%02d] ", i);
            for (; e != NULL; e = e->next) {
                printf ("%03d %s", e->val, e->key);
            }
            printf ("\n");
        }
    }
}

/* Using FNV-1a algorithm: http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function */
static uint32_t hash(char *s) {
    uint32_t hash = 2166136261;
    while (*s != '\0') {
        hash ^= *s++;
        hash *= 16777619;
    }
    return hash;
}

/* Add a string to the map. Return the existing mapping if it is already present. */
uint32_t Dedup_dedup (char *key) {
    uint32_t hc = hash(key) % SIZE;
    Entry *e = &(entries[hc]);
    if (e->key != NULL) {
        while (true) {
            if (strcmp(e->key, key) == 0) return e->val; // key already in set
            if (e->next == NULL) break;
            e = e->next;
        }
        e->next = malloc (sizeof (Entry));
        e = e->next;
    }
    e->key = key;
    e->val = n;
    e->next = NULL;
    n += 1;
    return e->val;
}

int test() {
    Dedup_init();
    Dedup_dedup("fifteen cans of soup");
    Dedup_dedup("the color of the sky");
    Dedup_dedup("tomorrow, it rains");
    Dedup_dedup("              ...espace");
    for (int i = 0; i < 100; ++i) Dedup_dedup("hello");
    printf("n = %d\n", n);
    printf("index of hello is %d\n", Dedup_dedup("hello"));
    Dedup_print();
    Dedup_clear();
    for (int i = 0; i < 100; ++i) Dedup_dedup("hello");
    printf("n = %d\n", n);
    printf("index of hello is %d\n", Dedup_dedup("hello"));
    Dedup_clear();
    return 0;
}
