#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "operators.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_operators_init (tdata_operators_t *ops, tdata_string_pool_t *pool)
{
    ops->operator_ids = NULL;
    ops->operator_urls = NULL;
    ops->operator_names = NULL;

    ops->pool = pool;

    ops->size = 0;
    ops->len = 0;

    return ret_ok;
}

ret_t
tdata_operators_mrproper (tdata_operators_t *ops)
{
    if (unlikely (ops->size == 0 && ops->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (ops->operator_ids)
        free (ops->operator_ids);
    
    if (ops->operator_urls)
        free (ops->operator_urls);
    
    if (ops->operator_names)
        free (ops->operator_names);
    
    return tdata_operators_init (ops, ops->pool);
}

ret_t
tdata_operators_ensure_size (tdata_operators_t *ops, opidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= ops->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (ops->operator_ids == NULL) {
        ops->operator_ids           = (uint32_t *) calloc (size, sizeof(uint32_t));
        ops->operator_urls          = (uint32_t *) calloc (size, sizeof(uint32_t));
        ops->operator_names         = (uint32_t *) calloc (size, sizeof(uint32_t));
        
        if (unlikely (ops->operator_ids == NULL ||
                      ops->operator_urls == NULL ||
                      ops->operator_names == NULL)) {
            return ret_nomem;
        }
        ops->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (ops->operator_ids, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    ops->operator_ids = (uint32_t *) p;

    p = realloc (ops->operator_urls, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    ops->operator_urls = (uint32_t *) p;

    p = realloc (ops->operator_names, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    ops->operator_names = (uint32_t *) p;

    ops->size = size;
    
    return ret_ok; 
}

ret_t
tdata_operators_add (tdata_operators_t *ops,
                     const char **id,
                     const char **url,
                     const char **name,
                     const opidx_t size,
                     opidx_t *offset)
{
    int available;

    if (unlikely (ops->size == 0 && ops->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = ops->size - ops->len;

    if ((opidx_t) available < size) {
        if (unlikely (tdata_operators_ensure_size (ops, (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    available = size;

    while (available) {
        opidx_t idx;

        available--;
        idx = ops->len + available;
        tdata_string_pool_add (ops->pool, id[available], strlen (id[available]) + 1, &ops->operator_ids[idx]);
        tdata_string_pool_add (ops->pool, url[available], strlen (url[available]) + 1, &ops->operator_urls[idx]);
        tdata_string_pool_add (ops->pool, name[available], strlen (name[available]) + 1, &ops->operator_names[idx]);
    }

    if (offset) {
        *offset = ops->len;
    }    

    ops->len += size;

    return ret_ok;
}
