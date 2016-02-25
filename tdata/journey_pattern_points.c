#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "journey_pattern_points.h"

#define REALLOC_EXTRA_SIZE     64

ret_t
tdata_journey_pattern_points_init (tdata_journey_pattern_points_t *jpps, tdata_stop_points_t *sps, tdata_string_pool_t *pool)
{
    jpps->journey_pattern_points = NULL;
    jpps->journey_pattern_point_headsigns = NULL;
    jpps->journey_pattern_point_attributes = NULL;

    jpps->sps = sps;
    jpps->pool = pool;

    jpps->size = 0;
    jpps->len = 0;

    return ret_ok;
}

ret_t
tdata_journey_pattern_points_fake (tdata_journey_pattern_points_t *jpps, const spidx_t *journey_pattern_points, const uint32_t *journey_pattern_point_headsigns, const uint8_t *journey_pattern_point_attributes, const uint32_t len)
{
    jpps->journey_pattern_points = (spidx_t *) journey_pattern_points;
    jpps->journey_pattern_point_headsigns = (uint32_t *) journey_pattern_point_headsigns;
    jpps->journey_pattern_point_attributes = (uint8_t *) journey_pattern_point_attributes;

    jpps->size = 0;
    jpps->len = len;

    return ret_ok;
}

ret_t
tdata_journey_pattern_points_mrproper (tdata_journey_pattern_points_t *jpps)
{
    if (unlikely (jpps->size == 0 && jpps->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (jpps->journey_pattern_points)
        free (jpps->journey_pattern_points);

    if (jpps->journey_pattern_point_headsigns)
        free (jpps->journey_pattern_point_headsigns);

    if (jpps->journey_pattern_point_attributes)
        free (jpps->journey_pattern_point_attributes);

    return tdata_journey_pattern_points_init (jpps, jpps->sps, jpps->pool);
}

ret_t
tdata_journey_pattern_points_ensure_size (tdata_journey_pattern_points_t *jpps, jppidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= jpps->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (jpps->journey_pattern_points == NULL) {
        jpps->journey_pattern_points = (spidx_t *) calloc (size, sizeof(spidx_t));
        jpps->journey_pattern_point_headsigns = (uint32_t *) calloc (size, sizeof(uint32_t));
        jpps->journey_pattern_point_attributes = (uint8_t *) calloc (size, sizeof(uint8_t));

        if (unlikely (jpps->journey_pattern_points == NULL ||
                      jpps->journey_pattern_point_headsigns == NULL ||
                      jpps->journey_pattern_point_attributes == NULL)) {
            return ret_nomem;
        }
        jpps->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (jpps->journey_pattern_points, sizeof(spidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jpps->journey_pattern_points = (spidx_t *) p;

    p = realloc (jpps->journey_pattern_point_headsigns, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jpps->journey_pattern_point_headsigns = (uint32_t *) p;

    p = realloc (jpps->journey_pattern_point_attributes, sizeof(uint8_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jpps->journey_pattern_point_attributes = (uint8_t *) p;

    jpps->size = size;

    return ret_ok;
}

ret_t
tdata_journey_pattern_points_add (tdata_journey_pattern_points_t *jpps,
                            const spidx_t *journey_pattern_points,
                            const char **journey_pattern_point_headsigns,
                            const uint8_t *journey_pattern_point_attributes,
                            const jppidx_t size,
                            jppidx_t *offset)
{
    int available;

    if (unlikely (jpps->size == 0 && jpps->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = jpps->size - jpps->len;

    if ((jppidx_t) available < size) {
        if (unlikely (tdata_journey_pattern_points_ensure_size (jpps, jpps->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (jpps->journey_pattern_points           + jpps->len, journey_pattern_points, sizeof(spidx_t) * size);
    memcpy (jpps->journey_pattern_point_attributes + jpps->len, journey_pattern_point_attributes, sizeof(uint8_t) * size);

    available = size;

    while (available) {
        jppidx_t idx;

        available--;
        idx = jpps->len + available;
        tdata_string_pool_add (jpps->pool, journey_pattern_point_headsigns[available], strlen (journey_pattern_point_headsigns[available]) + 1, &jpps->journey_pattern_point_headsigns[idx]);
    }

    if (offset) {
        *offset = jpps->len;
    }

    jpps->len += size;
    return ret_ok;
}
