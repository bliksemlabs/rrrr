#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "commercial_modes.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_commercial_modes_init (tdata_commercial_modes_t *cms, tdata_string_pool_t *pool)
{
    cms->commercial_mode_ids = NULL;
    cms->commercial_mode_names = NULL;

    cms->pool = pool;

    cms->size = 0;
    cms->len = 0;

    return ret_ok;
}

ret_t
tdata_commercial_modes_mrproper (tdata_commercial_modes_t *cms)
{
    if (unlikely (cms->size == 0 && cms->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (cms->commercial_mode_ids)
        free (cms->commercial_mode_ids);

    if (cms->commercial_mode_names)
        free (cms->commercial_mode_names);

    return tdata_commercial_modes_init (cms, cms->pool);
}

ret_t
tdata_commercial_modes_ensure_size (tdata_commercial_modes_t *cms, cmidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= cms->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (cms->commercial_mode_ids == NULL) {
        cms->commercial_mode_ids           = (uint32_t *) calloc (size, sizeof(uint32_t));
        cms->commercial_mode_names         = (uint32_t *) calloc (size, sizeof(uint32_t));

        if (unlikely (cms->commercial_mode_ids == NULL ||
                      cms->commercial_mode_names == NULL)) {
            return ret_nomem;
        }
        cms->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (cms->commercial_mode_ids, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    cms->commercial_mode_ids = (uint32_t *) p;

    p = realloc (cms->commercial_mode_names, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    cms->commercial_mode_names = (uint32_t *) p;

    cms->size = size;

    return ret_ok;
}

ret_t
tdata_commercial_modes_add (tdata_commercial_modes_t *cms,
                     const char **id,
                     const char **name,
                     const cmidx_t size,
                     cmidx_t *offset)
{
    int available;

    if (unlikely (cms->size == 0 && cms->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = cms->size - cms->len;

    if ((cmidx_t) available < size) {
        if (unlikely (tdata_commercial_modes_ensure_size (cms, cms->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    available = size;

    while (available) {
        cmidx_t idx;

        available--;
        idx = cms->len + available;
        tdata_string_pool_add (cms->pool, id[available], strlen (id[available]) + 1, &cms->commercial_mode_ids[idx]);
        tdata_string_pool_add (cms->pool, name[available], strlen (name[available]) + 1, &cms->commercial_mode_names[idx]);
    }

    if (offset) {
        *offset = cms->len;
    }

    cms->len += size;

    return ret_ok;
}
