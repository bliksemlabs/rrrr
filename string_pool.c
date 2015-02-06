#include "config.h"
#include "radixtree.h"
#include "rrrr_types.h"
#include "string.h"

uint32_t string_pool_append(char *pool, uint32_t *n_pool, radixtree_t *r, const char *str) {
    uint32_t location = radixtree_find (r, str);

    if (location == RADIXTREE_NONE) {
        size_t n = strlen(str);

        /* TODO: check if n_pool + n < max */
        strncpy (&pool[*n_pool], str, n);
        location = *n_pool;
        *n_pool += n;

        radixtree_insert(r, str, location);
    }

    return location;
}


uint32_t string_pool_append_scan(char *pool, uint32_t *n_pool, const char *str) {
    char *strings_end = pool + *n_pool;
    char *s = pool;
    uint32_t idx = 0;
    size_t len = strlen (str);

    while (s < strings_end) {
        size_t len2 = strlen (s);
        if (len == len2 && strcmp(pool, str) == 0) return idx;

        len2++;
        idx += len2;
        s += len2;
    }

    strncpy (&pool[*n_pool], str, len);
    idx = *n_pool;
    *n_pool += len;

    return idx;
}
