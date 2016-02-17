/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"

#ifdef RRRR_TDATA_IO_DYNAMIC

#include "string_pool.h"
#include "string.h"

uint32_t string_pool_append(char *pool, uint32_t *n_pool, radixtree_t *r, const char *str) {
    uint32_t location = radixtree_find_exact (r, str);

    if (location == RADIXTREE_NONE) {
        size_t n = strlen(str);

        /* TODO: check if n_pool + n < max */
        strncpy(&pool[*n_pool], str, n);
        pool[*n_pool+n] = '\0';
        location = *n_pool;
        *n_pool += n + 1;

        radixtree_insert(r, str, location);
    }

    return location;
}
#else
void string_pool_append_not_available();
#endif /* RRRR_TDATA_IO_DYNAMIC */
