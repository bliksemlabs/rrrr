#ifndef TDATA_STRING_POOL_H
#define TDATA_STRING_POOL_H

#include "tdata_common.h"
#include "common.h"

typedef struct {
    char *pool;
    uint32_t len;
    uint32_t size;
} tdata_string_pool_t;

ret_t tdata_string_pool_init (tdata_string_pool_t *pool);
ret_t tdata_string_pool_mrproper (tdata_string_pool_t *pool);
ret_t tdata_string_pool_ensure_size (tdata_string_pool_t *pool, uint32_t size);
ret_t tdata_string_pool_add (tdata_string_pool_t *pool, const char *c, size_t size, uint32_t *idx);
void  tdata_string_pool_fake (tdata_string_pool_t *pool, const char *p, const uint32_t len);

#endif
