/* Copyright 2013–2015 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "rrrr_types.h"
#include "radixtree.h"

uint32_t string_pool_append(char *pool, uint32_t *n_pool, radixtree_t *r, const char *str);
