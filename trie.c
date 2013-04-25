/* We took a reference implementation from: https://github.com/chriso/trie.c/ */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "trie.h"
#include "transitdata.h"

unsigned int global_index = 0;

inline trie_t *trie_init(void) {
    trie_t *t = (trie_t *) malloc(sizeof(trie_t));
    t->node = 0;
    t->index = -1;
    return t;
}

void trie_add(trie_t *t, char *word) {
    int c;
    while ((c = *word++)) {
        assert(c >= 0 && c < TRIE_SIZE);
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

int trie_exists(trie_t *t, char *word) {
    int c;
    while ((c = *word++)) {
        if (t->chars[c] == NULL) {
            return 0;
        }
        t = t->chars[c];
    }
    return t->chars[TRIE_SENTINEL] != NULL ? 1 : 0;
}

int trie_prefix(trie_t *t, char *prefix) {
    int c;
    while ((c = *prefix++)) {
        if (t->chars[c] == NULL) {
            return 0;
        }
        t = t->chars[c];
    }
    return t->node;
}

unsigned int trie_complete(trie_t *t, char *prefix) {
    int extra = 0;
    int c;
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
            putchar(c);
            t = t->chars[c];
            c = 0;
            extra++;
        }
    }

    return t->index;
}

int trie_load(trie_t *t, transit_data_t *td) {
    trie_t *root = t;
    int words = 0;

    for (int i = 0; i < td->nstops; i++) {
        char *stopname = transit_data_stop_id_for_index(td, i);
        int c, word_len = strlen(stopname);

        for (int j = 0; j <= word_len; j++) {
            /* lowercase */
            c = stopname[j] >= 'A' && stopname[j] <= 'Z' ? stopname[j] | 0x60 : stopname[j];
            if (c == '\0') {
                t->index = global_index;
                global_index++;
                t->chars[TRIE_SENTINEL] = trie_init();
                words++;
                t = root;
            } else {
                if (c >= 0 && c < TRIE_SIZE) {
                    assert(c >= 0 && c < TRIE_SIZE);
                    if (t->chars[c] == NULL) {
                        t->chars[c] = trie_init();
                    }
                    t->node = 1;
                    t = t->chars[c];
                }
            }
        }
        if (t != root && word_len > 0) {
            t->index = global_index;
            global_index++;
            t->chars[TRIE_SENTINEL] = trie_init();
        }
    }
    return words;
}

void trie_strip(trie_t *t, char *src, char *dest) {
    if (src == NULL) {
        return;
    }
    if (dest == NULL) {
        dest = src;
    }
    int c, i = 0, last_break = 0, in_trie = 1;
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
    int i;
    for (i = 0; i < TRIE_SIZE; i++) {
        if (t->chars[i] != NULL) {
            trie_free(t->chars[i]);
        }
    }
    free(t);
}
