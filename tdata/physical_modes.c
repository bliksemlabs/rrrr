#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "physical_modes.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_physical_modes_init (tdata_physical_modes_t *pms, tdata_string_pool_t *pool)
{
    pms->physical_mode_ids = NULL;
    pms->physical_mode_urls = NULL;
    pms->physical_mode_names = NULL;

    pms->pool = pool;

    pms->size = 0;
    pms->len = 0;

    return ret_ok;
}

ret_t
tdata_physical_modes_mrproper (tdata_physical_modes_t *pms)
{
    if (unlikely (pms->size == 0 && pms->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (pms->physical_mode_ids)
        free (pms->physical_mode_ids);
    
    if (pms->physical_mode_urls)
        free (pms->physical_mode_urls);
    
    if (pms->physical_mode_names)
        free (pms->physical_mode_names);
    
    return tdata_physical_modes_init (pms, pms->pool);
}

ret_t
tdata_physical_modes_ensure_size (tdata_physical_modes_t *pms, pmidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= pms->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (pms->physical_mode_ids == NULL) {
        pms->physical_mode_ids           = (uint32_t *) calloc (size, sizeof(uint32_t));
        pms->physical_mode_urls          = (uint32_t *) calloc (size, sizeof(uint32_t));
        pms->physical_mode_names         = (uint32_t *) calloc (size, sizeof(uint32_t));
        
        if (unlikely (pms->physical_mode_ids == NULL ||
                      pms->physical_mode_urls == NULL ||
                      pms->physical_mode_names == NULL)) {
            return ret_nomem;
        }
        pms->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (pms->physical_mode_ids, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    pms->physical_mode_ids = (uint32_t *) p;

    p = realloc (pms->physical_mode_urls, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    pms->physical_mode_urls = (uint32_t *) p;

    p = realloc (pms->physical_mode_names, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    pms->physical_mode_names = (uint32_t *) p;

    pms->size = size;
    
    return ret_ok; 
}

ret_t
tdata_physical_modes_add (tdata_physical_modes_t *pms,
                     const char **id,
                     const char **url,
                     const char **name,
                     const pmidx_t size,
                     pmidx_t *offset)
{
    int available;

    if (unlikely (pms->size == 0 && pms->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = pms->size - pms->len;

    if ((pmidx_t) available < size) {
        if (unlikely (tdata_physical_modes_ensure_size (pms, (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    available = size;

    while (available) {
        pmidx_t idx;

        available--;
        idx = pms->len + available;
        tdata_string_pool_add (pms->pool, id[available], strlen (id[available]) + 1, &pms->physical_mode_ids[idx]);
        tdata_string_pool_add (pms->pool, url[available], strlen (url[available]) + 1, &pms->physical_mode_urls[idx]);
        tdata_string_pool_add (pms->pool, name[available], strlen (name[available]) + 1, &pms->physical_mode_names[idx]);
    }

    if (offset) {
        *offset = pms->len;
    }    

    pms->len += size;

    return ret_ok;
}
