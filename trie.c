/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* We took a reference implementation from: https://github.com/chriso/trie.c/ */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "trie.h"
#include "tdata.h"

uint32_t global_index = 0;

inline trie_t *trie_init(void) {
    trie_t *t = (trie_t *) malloc(sizeof(trie_t));
    memset(t, 0, sizeof(trie_t));
    t->node = 0;
    t->index = -1;
    return t;
}

void trie_add(trie_t *t, char *word) {
    uint32_t c;
    while ((c = *word++)) {
        assert(c < TRIE_SIZE);
        if (t->chars[c] == NULL) {
            t->chars[c] = trie_init();
        }
        t->node = 1;
        t = t->chars[c];
    }
    t->node = 1;
    t->index = global_index;
    global_index++;
    t->chars[TRIE_SENTINEL] = trie_init();
}

uint32_t trie_exists(trie_t *t, char *word) {
    uint32_t c;
    while ((c = *word++)) {
        if (t->chars[c] == NULL) {
            return 0;
        }
        t = t->chars[c];
    }
    return t->chars[TRIE_SENTINEL] != NULL ? 1 : 0;
}

uint32_t trie_prefix(trie_t *t, char *prefix) {
    uint32_t c;
    while ((c = *prefix++)) {
        if (t->chars[c] == NULL) {
            return 0;
        }
        t = t->chars[c];
    }
    return t->node;
}

uint32_t trie_complete(trie_t *t, char *prefix, char *suffix) {
    uint32_t extra = 0;
    uint32_t c;
    while ((c = *prefix++)) {
        if (t->chars[c] == NULL) {
            return -1;
        }
        t = t->chars[c];
    }

    c = 0;
    while (c < TRIE_SIZE && t->node == 1) {
        c++;
        if (t->chars[c] != NULL) {
            suffix[extra] = c;
            t = t->chars[c];
            c = 0;
            extra++;
        }
    }

    suffix[extra] = 0;

    return t->index;
}

uint32_t trie_load(trie_t *t, tdata_t *td) {
    trie_t *root = t;

    for (uint32_t i = 0; i < td->n_stops; i++) {
        char *stopname = tdata_stop_name_for_index(td, i);
        uint32_t c, word_len = strlen(stopname);

        for (uint32_t j = 0; j < word_len; j++) {
            /* lowercase */
            c = stopname[j] >= 'A' && stopname[j] <= 'Z' ? stopname[j] | 0x60 : stopname[j];
            assert(c < TRIE_SIZE);
            if (t->chars[c] == NULL) {
                t->chars[c] = trie_init();
            }
            t->node = 1;
            t = t->chars[c];
        }
        
        t->index = global_index;
        global_index++;
//        t->chars[TRIE_SENTINEL] = trie_init();
        t = root;
    }
    return td->n_stops;
}

void trie_strip(trie_t *t, char *src, char *dest) {
    if (src == NULL) {
        return;
    }
    if (dest == NULL) {
        dest = src;
    }
    uint32_t c, i = 0, last_break = 0, in_trie = 1;
    trie_t *root = t;

    while ((c = dest[i++] = *src++)) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
            t = root;
            if (in_trie) {
                i = last_break;
            } else {
                in_trie = 1;
                last_break = i;
            }
            continue;
        }
        if (!in_trie) {
            continue;
        }
        if (t->chars[c] == NULL) {
            in_trie = 0;
        } else {
            t = t->chars[c];
            in_trie = 1;
        }
    }
}

void trie_free(trie_t *t) {
    uint32_t i;
    for (i = 0; i < TRIE_SIZE; i++) {
        if (t->chars[i] != NULL) {
            trie_free(t->chars[i]);
        }
    }
    free(t);
}
