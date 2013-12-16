/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

#ifndef _TRIE_H_
#define _TRIE_H_

#define TRIE_SIZE 128
#define TRIE_SENTINEL 0

#include "tdata.h"

typedef struct _trie_t {
    struct _trie_t *chars[TRIE_SIZE];
    unsigned char node;
    int32_t index;
} trie_t;

trie_t *trie_init(void);
void trie_add(trie_t *, char *);
uint32_t trie_exists(trie_t *, char *);
uint32_t trie_prefix(trie_t *, char *);
uint32_t trie_complete(trie_t *, char *, char *);
uint32_t trie_load(trie_t *, tdata_t *);
void trie_strip(trie_t *, char *, char *);
void trie_free(trie_t *);

#define trie_step(t,c) (t = (t == NULL || t->chars[c] == NULL ? NULL : t->chars[c]))
#define trie_word(t) (t != NULL && t->chars[TRIE_SENTINEL] != NULL)

#endif
