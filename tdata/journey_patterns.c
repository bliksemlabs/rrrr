#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "journey_patterns.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_journey_patterns_init (tdata_journey_patterns_t *jps,
                             tdata_commercial_modes_t *cms,
                             tdata_routes_t *routes,
                             tdata_journey_pattern_points_t *jpps,
                             tdata_vehicle_journeys_t *vjs)
{
    jps->journey_pattern_point_offset = NULL;
    jps->vj_index = NULL;
    jps->n_stops = NULL;
    jps->n_vjs = NULL;
    jps->attributes = NULL;
    jps->route_index = NULL;
    jps->commercial_mode_for_jp = NULL;

    jps->cms = cms;
    jps->routes = routes;
    jps->jpps = jpps;
    jps->vjs = vjs;

    jps->size = 0;
    jps->len = 0;

    return ret_ok;
}

ret_t
tdata_journey_patterns_mrproper (tdata_journey_patterns_t *jps)
{
    if (unlikely (jps->size == 0 && jps->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (jps->journey_pattern_point_offset)
        free (jps->journey_pattern_point_offset);

    if (jps->vj_index)
        free (jps->vj_index);

    if (jps->n_stops)
        free (jps->n_stops);

    if (jps->n_vjs)
        free (jps->n_vjs);

    if (jps->attributes)
        free (jps->attributes);

    if (jps->route_index)
        free (jps->route_index);

    if (jps->commercial_mode_for_jp)
        free (jps->commercial_mode_for_jp);

    return tdata_journey_patterns_init (jps, jps->cms, jps->routes, jps->jpps, jps->vjs);
}

ret_t
tdata_journey_patterns_ensure_size (tdata_journey_patterns_t *jps, spidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= jps->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (jps->journey_pattern_point_offset == NULL) {
        jps->journey_pattern_point_offset = (uint32_t *) calloc (size, sizeof(uint32_t));;
        jps->vj_index = (vjidx_t *) calloc (size, sizeof(vjidx_t));;
        jps->n_stops = (jppidx_t *) calloc (size, sizeof(jppidx_t));;
        jps->n_vjs = (jp_vjoffset_t *) calloc (size, sizeof(jp_vjoffset_t));;
        jps->attributes = (uint16_t *) calloc (size, sizeof(uint16_t));;
        jps->route_index = (routeidx_t *) calloc (size, sizeof(routeidx_t));;
        jps->commercial_mode_for_jp = (cmidx_t *) calloc (size, sizeof(cmidx_t));;

        if (unlikely (jps->journey_pattern_point_offset == NULL ||
                      jps->vj_index == NULL ||
                      jps->n_stops == NULL ||
                      jps->n_vjs == NULL ||
                      jps->attributes == NULL ||
                      jps->route_index == NULL ||
                      jps->commercial_mode_for_jp == NULL)) {
            return ret_nomem;
        }
        jps->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (jps->journey_pattern_point_offset, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jps->journey_pattern_point_offset = (uint32_t *) p;

    p = realloc (jps->vj_index, sizeof(vjidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jps->vj_index = (vjidx_t *) p;

    p = realloc (jps->n_stops, sizeof(jppidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jps->n_stops = (jppidx_t *) p;

    p = realloc (jps->n_vjs, sizeof(jp_vjoffset_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jps->n_vjs = (jp_vjoffset_t *) p;

    p = realloc (jps->attributes, sizeof(uint16_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jps->attributes = (uint16_t *) p;

    p = realloc (jps->route_index, sizeof(routeidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jps->route_index = (routeidx_t *) p;

    p = realloc (jps->commercial_mode_for_jp, sizeof(cmidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    jps->commercial_mode_for_jp = (cmidx_t *) p;

    jps->size = size;

    return ret_ok;
}

ret_t
tdata_journey_patterns_add (tdata_journey_patterns_t *jps,
                            const uint32_t *journey_pattern_point_offset,
                            const vjidx_t *vj_index,
                            const jppidx_t *n_stops,
                            const jp_vjoffset_t *n_vjs,
                            const uint16_t *attributes,
                            const routeidx_t *route_index,
                            const cmidx_t *commercial_mode_for_jp,
                            const jpidx_t size,
                            jpidx_t *offset)
{
    int available;

    if (unlikely (jps->size == 0 && jps->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = jps->size - jps->len;

    if ((jpidx_t) available < size) {
        if (unlikely (tdata_journey_patterns_ensure_size (jps, jps->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (jps->journey_pattern_point_offset + jps->len, journey_pattern_point_offset, sizeof(uint32_t) * size);
    memcpy (jps->vj_index + jps->len, vj_index, sizeof(vjidx_t) * size );
    memcpy (jps->n_stops + jps->len, n_stops, sizeof(jppidx_t) * size);
    memcpy (jps->n_vjs + jps->len, n_vjs, sizeof(uint16_t) * size);
    memcpy (jps->attributes + jps->len, attributes, sizeof(uint16_t) * size);
    memcpy (jps->route_index + jps->len, route_index, sizeof(routeidx_t) * size);
    memcpy (jps->commercial_mode_for_jp + jps->len, commercial_mode_for_jp, sizeof(cmidx_t) * size);

    if (offset) {
        *offset = jps->len;
    }

    jps->len += size;

    return ret_ok;
}
