#ifndef _TRIE_H_
#define _TRIE_H_

#define TRIE_SIZE 128
#define TRIE_SENTINEL 0

#include "transitdata.h"

typedef struct _trie_t {
    struct _trie_t *chars[TRIE_SIZE];
    unsigned char node;
    unsigned int index;
} trie_t;

trie_t *trie_init(void);
void trie_add(trie_t *, char *);
int trie_exists(trie_t *, char *);
int trie_prefix(trie_t *, char *);
unsigned int trie_complete(trie_t *, char *);
int trie_load(trie_t *, transit_data_t *);
void trie_strip(trie_t *, char *, char *);
void trie_free(trie_t *);

#define trie_step(t,c) (t = (t == NULL || t->chars[c] == NULL ? NULL : t->chars[c]))
#define trie_word(t) (t != NULL && t->chars[TRIE_SENTINEL] != NULL)

#endif
