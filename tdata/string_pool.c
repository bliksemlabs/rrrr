#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "string_pool.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_string_pool_init (tdata_string_pool_t *pool)
{
    pool->pool = NULL;

    pool->size = 0;
    pool->len = 0;

    return ret_ok;
}

ret_t
tdata_string_pool_mrproper (tdata_string_pool_t *pool)
{
    if (unlikely (pool->size == 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (pool->pool)
        free (pool->pool);
    
    return tdata_string_pool_init (pool);
}

ret_t
tdata_string_pool_ensure_size (tdata_string_pool_t *pool, uint32_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= pool->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (pool->pool == NULL) {
        pool->pool = (char *) calloc (size, sizeof(char));
        
        if (unlikely (pool->pool == NULL)) {
            return ret_nomem;
        }
        pool->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (pool->pool, sizeof(char) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    pool->pool = (char *) p;

    pool->size = size;

    return ret_ok; 
}

ret_t
tdata_string_pool_add (tdata_string_pool_t *pool, const char *c, size_t size, uint32_t *idx)
{
    int available;

    if (unlikely (pool->size == 0 && pool->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = pool->size - pool->len;

    if ((uint32_t) available < size) {
        if (unlikely (tdata_string_pool_ensure_size (pool, (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (pool->pool + pool->len, c, size);

    assert (idx != NULL);
    *idx = pool->len;

    pool->len += size;

    return ret_ok;
}
