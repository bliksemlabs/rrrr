#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "stop_areas.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_stop_areas_init (tdata_stop_areas_t *sas, tdata_string_pool_t *pool)
{
    sas->stop_area_ids = NULL;
    sas->stop_area_coords = NULL;
    sas->stop_area_nameidx = NULL;
    sas->stop_area_timezones = NULL;

    sas->pool = pool;

    sas->size = 0;
    sas->len = 0;

    return ret_ok;
}

ret_t
tdata_stop_areas_mrproper (tdata_stop_areas_t *sas)
{
    if (unlikely (sas->size == 0 && sas->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (sas->stop_area_ids)
        free (sas->stop_area_ids);

    if (sas->stop_area_coords)
        free (sas->stop_area_coords);

    if (sas->stop_area_nameidx)
        free (sas->stop_area_nameidx);

    if (sas->stop_area_timezones)
        free (sas->stop_area_timezones);

    return tdata_stop_areas_init (sas, sas->pool);
}

ret_t
tdata_stop_areas_ensure_size (tdata_stop_areas_t *sas, spidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= sas->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (sas->stop_area_ids == NULL) {
        sas->stop_area_ids           = (uint32_t *) calloc (size, sizeof(uint32_t));
        sas->stop_area_coords        = (latlon_t *) calloc (size, sizeof(latlon_t));
        sas->stop_area_nameidx       = (uint32_t *) calloc (size, sizeof(uint32_t));
        sas->stop_area_timezones     = (uint32_t *) calloc (size, sizeof(uint32_t));

        if (unlikely (sas->stop_area_ids == NULL ||
                      sas->stop_area_coords == NULL ||
                      sas->stop_area_nameidx == NULL ||
                      sas->stop_area_timezones == NULL)) {
            return ret_nomem;
        }
        sas->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (sas->stop_area_ids, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sas->stop_area_ids = (uint32_t *) p;

    p = realloc (sas->stop_area_coords, sizeof(latlon_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sas->stop_area_coords = (latlon_t *) p;

    p = realloc (sas->stop_area_nameidx, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sas->stop_area_nameidx = (uint32_t *) p;

    p = realloc (sas->stop_area_timezones, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sas->stop_area_timezones = (uint32_t *) p;

    sas->size = size;

    return ret_ok;
}

ret_t
tdata_stop_areas_add (tdata_stop_areas_t *sas,
                      const char **id,
                      const char **name,
                      const latlon_t *coord,
                      const char **timezone,
                      const spidx_t size,
                      spidx_t *offset)
{
    int available;

    if (unlikely (sas->size == 0 && sas->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = sas->size - sas->len;

    if ((spidx_t) available < size) {
        if (unlikely (tdata_stop_areas_ensure_size (sas, sas->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (sas->stop_area_coords + sas->len, coord, sizeof(latlon_t) * size);

    available = size;

    while (available) {
        spidx_t idx;

        available--;
        idx = sas->len + available;
        tdata_string_pool_add (sas->pool, id[available], strlen (id[available]) + 1, &sas->stop_area_ids[idx]);
        tdata_string_pool_add (sas->pool, name[available], strlen (name[available]) + 1, &sas->stop_area_nameidx[idx]);
        tdata_string_pool_add (sas->pool, timezone[available], strlen (timezone[available]) + 1, &sas->stop_area_timezones[idx]);
    }

    if (offset) {
        *offset = sas->len;
    }

    sas->len += size;

    return ret_ok;
}
