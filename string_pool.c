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

