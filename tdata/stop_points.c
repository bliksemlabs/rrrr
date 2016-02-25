#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "stop_points.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_stop_points_init (tdata_stop_points_t *sps, tdata_stop_areas_t *sas, tdata_transfers_t *transfers, tdata_string_pool_t *pool)
{
    sps->platformcodes = NULL;
    sps->stop_point_ids = NULL;
    sps->stop_point_coords = NULL;
    sps->stop_point_nameidx = NULL;
    sps->stop_point_waittime = NULL;
    sps->stop_point_attributes = NULL;
    sps->stop_area_for_stop_point = NULL;
    sps->transfers_offset = NULL;

    sps->sas = sas;
    sps->transfers = transfers;
    sps->pool = pool;

    sps->size = 0;
    sps->len = 0;

    return ret_ok;
}

ret_t
tdata_stop_points_mrproper (tdata_stop_points_t *sps)
{
    if (unlikely (sps->size == 0 && sps->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (sps->platformcodes)
        free (sps->platformcodes);

    if (sps->stop_point_ids)
        free (sps->stop_point_ids);

    if (sps->stop_point_coords)
        free (sps->stop_point_coords);

    if (sps->stop_point_nameidx)
        free (sps->stop_point_nameidx);

    if (sps->stop_point_waittime)
        free (sps->stop_point_waittime);

    if (sps->stop_point_attributes)
        free (sps->stop_point_attributes);

    if (sps->stop_area_for_stop_point)
        free (sps->stop_area_for_stop_point);

    if (sps->transfers_offset)
        free (sps->transfers_offset);

    return tdata_stop_points_init (sps, sps->sas, sps->transfers, sps->pool);
}

ret_t
tdata_stop_points_ensure_size (tdata_stop_points_t *sps, spidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= sps->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (sps->stop_point_ids == NULL) {
        sps->platformcodes            = (uint32_t *) calloc (size, sizeof(uint32_t));
        sps->stop_point_ids           = (uint32_t *) calloc (size, sizeof(uint32_t));
        sps->stop_point_coords        = (latlon_t *) calloc (size, sizeof(latlon_t));
        sps->stop_point_nameidx       = (uint32_t *) calloc (size, sizeof(uint32_t));
        sps->stop_point_waittime      = (rtime_t *)  calloc (size, sizeof(rtime_t));
        sps->stop_point_attributes    = (uint8_t *)  calloc (size, sizeof(uint8_t));
        sps->stop_area_for_stop_point = (spidx_t *)  calloc (size, sizeof(spidx_t));
        sps->transfers_offset     = (uint32_t *) calloc (size + 1, sizeof(uint32_t));

        if (unlikely (sps->platformcodes == NULL ||
                      sps->stop_point_ids == NULL ||
                      sps->stop_point_coords == NULL ||
                      sps->stop_point_nameidx == NULL ||
                      sps->stop_point_waittime == NULL ||
                      sps->stop_point_attributes == NULL ||
                      sps->stop_area_for_stop_point == NULL ||
                      sps->transfers_offset == NULL)) {
            return ret_nomem;
        }
        sps->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (sps->platformcodes, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->platformcodes = (uint32_t *) p;

    p = realloc (sps->stop_point_ids, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->stop_point_ids = (uint32_t *) p;

    p = realloc (sps->stop_point_coords, sizeof(latlon_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->stop_point_coords = (latlon_t *) p;

    p = realloc (sps->stop_point_nameidx, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->stop_point_nameidx = (uint32_t *) p;

    p = realloc (sps->stop_point_waittime, sizeof(rtime_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->stop_point_waittime = (rtime_t *) p;

    p = realloc (sps->stop_point_attributes, sizeof(uint8_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->stop_point_attributes = (uint8_t *) p;

    p = realloc (sps->stop_area_for_stop_point, sizeof(spidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->stop_area_for_stop_point = (spidx_t *) p;

    p = realloc (sps->transfers_offset, sizeof(uint32_t) * (size + 1));
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sps->transfers_offset = (uint32_t *) p;

    sps->size = size;

    return ret_ok;
}

ret_t
tdata_stop_points_add (tdata_stop_points_t *sps,
                      const char **id,
                      const char **name,
                      const char **platformcode,
                      const latlon_t *coord,
                      const rtime_t *waittime,
                      const spidx_t *stop_area,
                      const uint8_t *attributes,
                      const spidx_t size,
                      spidx_t *offset)
{
    int available;

    if (unlikely (sps->size == 0 && sps->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = sps->size - sps->len;

    if ((spidx_t) available < size) {
        if (unlikely (tdata_stop_points_ensure_size (sps, sps->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (sps->stop_point_coords + sps->len, coord, sizeof(latlon_t) * size);
    memcpy (sps->stop_point_waittime + sps->len, waittime, sizeof(rtime_t) * size);
    memcpy (sps->stop_point_attributes + sps->len, attributes, sizeof(uint8_t) * size);
    memcpy (sps->stop_area_for_stop_point + sps->len, stop_area, sizeof(spidx_t) * size);

    available = size;

    while (available) {
        spidx_t idx;

        available--;
        idx = sps->len + available;
        tdata_string_pool_add (sps->pool, platformcode[available], strlen (platformcode[available]) + 1, &sps->platformcodes[idx]);
        tdata_string_pool_add (sps->pool, id[available], strlen (id[available]) + 1, &sps->stop_point_ids[idx]);
        tdata_string_pool_add (sps->pool, name[available], strlen (name[available]) + 1, &sps->stop_point_nameidx[idx]);
    }

    if (offset) {
        *offset = sps->len;
    }

    sps->len += size;

    return ret_ok;
}
