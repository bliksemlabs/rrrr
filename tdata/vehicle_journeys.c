#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "vehicle_journeys.h"

#define REALLOC_EXTRA_SIZE     128

ret_t
tdata_vehicle_journeys_init (tdata_vehicle_journeys_t *vjs,
                             tdata_vehicle_transfers_t *vts,
                             tdata_stop_times_t *sts,
                             tdata_string_pool_t *pool)
{
    vjs->stop_times_offset = NULL;
    vjs->begin_time = NULL;
    vjs->vj_attributes = NULL;
    vjs->vj_ids = NULL;
    vjs->vj_active = NULL;
    vjs->vj_time_offsets = NULL;
    vjs->vehicle_journey_transfers_forward = NULL;
    vjs->vehicle_journey_transfers_backward = NULL;

    /* TODO */
    vjs->vj_transfers_forward_offset = NULL;
    vjs->vj_transfers_backward_offset = NULL;
    vjs->n_transfers_forward = NULL;
    vjs->n_transfers_backward = NULL;

    vjs->vts = vts;
    vjs->sts = sts;
    vjs->pool = pool;

    vjs->size = 0;
    vjs->len = 0;

    return ret_ok;
}

ret_t
tdata_vehicle_journeys_fake (tdata_vehicle_journeys_t *vjs,
                             const uint32_t *stop_times_offset,
                             const rtime_t *begin_time,
                             const vj_attribute_mask_t *vj_attributes,
                             const uint32_t *vj_ids,
                             const calendar_t *vj_active,
                             const int8_t *vj_time_offsets,
                             const vehicle_journey_ref_t *vehicle_journey_transfers_forward,
                             const vehicle_journey_ref_t *vehicle_journey_transfers_backward,
                             const uint32_t *vj_transfers_forward_offset,
                             const uint32_t *vj_transfers_backward_offset,
                             const uint8_t *n_transfers_forward,
                             const uint8_t *n_transfers_backward,
                             const uint32_t len)
{
    if (len > ((vjidx_t) -1))
        return ret_error;

    vjs->stop_times_offset = (uint32_t *) stop_times_offset;
    vjs->begin_time        = (rtime_t *) begin_time;
    vjs->vj_attributes     = (vj_attribute_mask_t *) vj_attributes;
    vjs->vj_ids            = (uint32_t *) vj_ids;
    vjs->vj_active         = (calendar_t *) vj_active;
    vjs->vj_time_offsets    = (int8_t *) vj_time_offsets;
    vjs->vehicle_journey_transfers_forward = (vehicle_journey_ref_t *) vehicle_journey_transfers_forward;
    vjs->vehicle_journey_transfers_backward = (vehicle_journey_ref_t *) vehicle_journey_transfers_backward;

    vjs->vj_transfers_forward_offset = (uint32_t *) vj_transfers_forward_offset;
    vjs->vj_transfers_backward_offset = (uint32_t *) vj_transfers_backward_offset;
    vjs->n_transfers_forward  = (uint8_t *) n_transfers_forward;
    vjs->n_transfers_backward = (uint8_t *) n_transfers_backward;

    vjs->size = 0;
    vjs->len = len;

    return ret_ok;
}

ret_t
tdata_vehicle_journeys_mrproper (tdata_vehicle_journeys_t *vjs)
{
    if (unlikely (vjs->size == 0 && vjs->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (vjs->stop_times_offset)
        free (vjs->stop_times_offset);

    if (vjs->begin_time)
        free (vjs->begin_time);

    if (vjs->vj_attributes)
        free (vjs->vj_attributes);

    if (vjs->vj_ids)
        free (vjs->vj_ids);

    if (vjs->vj_active)
        free (vjs->vj_active);

    if (vjs->vj_time_offsets)
        free (vjs->vj_time_offsets);

    if (vjs->vehicle_journey_transfers_forward)
        free (vjs->vehicle_journey_transfers_forward);

    if (vjs->vehicle_journey_transfers_backward)
        free (vjs->vehicle_journey_transfers_backward);

    if (vjs->vj_transfers_forward_offset)
        free (vjs->vj_transfers_forward_offset);

    if (vjs->vj_transfers_backward_offset)
        free (vjs->vj_transfers_backward_offset);

    if (vjs->n_transfers_forward)
        free (vjs->n_transfers_forward);

    if (vjs->n_transfers_backward)
        free (vjs->n_transfers_backward);

    return tdata_vehicle_journeys_init (vjs, vjs->vts, vjs->sts, vjs->pool);
}

ret_t
tdata_vehicle_journeys_ensure_size (tdata_vehicle_journeys_t *vjs, vjidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= vjs->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (vjs->vj_ids == NULL) {
        vjs->stop_times_offset = (uint32_t *) calloc (size, sizeof(uint32_t));
        vjs->begin_time        = (rtime_t *)  calloc (size, sizeof(rtime_t));
        vjs->vj_attributes     = (vj_attribute_mask_t *)  calloc (size, sizeof(vj_attribute_mask_t));
        vjs->vj_ids            = (uint32_t *)  calloc (size, sizeof(uint32_t));
        vjs->vj_active         = (calendar_t *) calloc (size, sizeof(calendar_t));
        vjs->vj_time_offsets    = (int8_t *) calloc (size, sizeof(int8_t));
        vjs->vehicle_journey_transfers_forward = (vehicle_journey_ref_t *) calloc (size, sizeof(vehicle_journey_ref_t));
        vjs->vehicle_journey_transfers_backward = (vehicle_journey_ref_t *) calloc (size, sizeof(vehicle_journey_ref_t));

        vjs->vj_transfers_forward_offset = (uint32_t *) calloc (size, sizeof(uint32_t));
        vjs->vj_transfers_backward_offset = (uint32_t *) calloc (size, sizeof(uint32_t));
        vjs->n_transfers_forward  = (uint8_t *) calloc (size, sizeof(uint8_t));
        vjs->n_transfers_backward = (uint8_t *) calloc (size, sizeof(uint8_t));

        if (unlikely (vjs->stop_times_offset == NULL ||
                      vjs->begin_time == NULL ||
                      vjs->vj_attributes == NULL ||
                      vjs->vj_ids == NULL ||
                      vjs->vj_active == NULL ||
                      vjs->vj_time_offsets == NULL ||
                      vjs->vehicle_journey_transfers_forward == NULL ||
                      vjs->vehicle_journey_transfers_backward == NULL ||
                      vjs->vj_transfers_forward_offset == NULL ||
                      vjs->vj_transfers_backward_offset == NULL ||
                      vjs->n_transfers_forward == NULL ||
                      vjs->n_transfers_backward == NULL)) {
            return ret_nomem;
        }
        vjs->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (vjs->stop_times_offset, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->stop_times_offset = (uint32_t *) p;

    p = realloc (vjs->begin_time, sizeof(rtime_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->begin_time = (rtime_t *) p;

    p = realloc (vjs->vj_attributes, sizeof(vj_attribute_mask_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vj_attributes = (vj_attribute_mask_t *) p;

    p = realloc (vjs->vj_ids, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vj_ids = (uint32_t *) p;

    p = realloc (vjs->vj_active, sizeof(calendar_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vj_active = (calendar_t *) p;

    p = realloc (vjs->vj_time_offsets, sizeof(int8_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vj_time_offsets = (int8_t *) p;

    p = realloc (vjs->vehicle_journey_transfers_forward, sizeof(vehicle_journey_ref_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vehicle_journey_transfers_forward = (vehicle_journey_ref_t *) p;

    p = realloc (vjs->vehicle_journey_transfers_backward, sizeof(vehicle_journey_ref_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vehicle_journey_transfers_backward = (vehicle_journey_ref_t *) p;

    p = realloc (vjs->vj_transfers_forward_offset, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vj_transfers_forward_offset = (uint32_t *) p;

    p = realloc (vjs->vj_transfers_backward_offset, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->vj_transfers_backward_offset = (uint32_t *) p;

    p = realloc (vjs->n_transfers_forward, sizeof(uint8_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->n_transfers_forward = (uint8_t *) p;

    p = realloc (vjs->n_transfers_backward, sizeof(uint8_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vjs->n_transfers_backward = (uint8_t *) p;

    vjs->size = size;

    return ret_ok;
}

ret_t
tdata_vehicle_journeys_add (tdata_vehicle_journeys_t *vjs,
                             const char **id,
                             const uint32_t *stop_times_offset,
                             const rtime_t *begin_time,
                             const vj_attribute_mask_t *vj_attributes,
                             const calendar_t *vj_active,
                             const int8_t *vj_time_offsets,
                             const vehicle_journey_ref_t *vehicle_journey_transfers_forward,
                             const vehicle_journey_ref_t *vehicle_journey_transfers_backward,
                             const uint32_t *vj_transfers_forward_offset,
                             const uint32_t *vj_transfers_backward_offset,
                             const uint8_t *n_transfers_forward,
                             const uint8_t *n_transfers_backward,
                             const vjidx_t size,
                             vjidx_t *offset) {
    int available;

    if (unlikely (vjs->size == 0 && vjs->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = vjs->size - vjs->len;

    if ((vjidx_t) available < size) {
        if (unlikely (tdata_vehicle_journeys_ensure_size (vjs, vjs->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (vjs->stop_times_offset + vjs->len, stop_times_offset, sizeof(uint32_t) * size);
    memcpy (vjs->begin_time + vjs->len, begin_time, sizeof(rtime_t) * size);

    memcpy (vjs->vj_attributes + vjs->len, vj_attributes, sizeof(vj_attribute_mask_t) * size);
    memcpy (vjs->vj_active + vjs->len, vj_active, sizeof(calendar_t) * size);
    memcpy (vjs->vj_time_offsets + vjs->len, vj_time_offsets, sizeof(int8_t) * size);
    memcpy (vjs->vehicle_journey_transfers_forward + vjs->len, vehicle_journey_transfers_forward, sizeof(vehicle_journey_ref_t) * size);
    memcpy (vjs->vehicle_journey_transfers_backward + vjs->len, vehicle_journey_transfers_backward, sizeof(vehicle_journey_ref_t) * size);
    memcpy (vjs->vj_transfers_forward_offset + vjs->len, vj_transfers_forward_offset, sizeof(uint32_t) * size);
    memcpy (vjs->vj_transfers_backward_offset + vjs->len, vj_transfers_backward_offset, sizeof(uint32_t) * size);
    memcpy (vjs->n_transfers_forward + vjs->len, n_transfers_forward, sizeof(uint8_t) * size);
    memcpy (vjs->n_transfers_backward + vjs->len, n_transfers_backward, sizeof(uint8_t) * size);

    available = size;

    while (available) {
        vjidx_t idx;

        available--;
        idx = vjs->len + available;
        tdata_string_pool_add (vjs->pool, id[available], strlen (id[available]) + 1, &vjs->vj_ids[idx]);
    }

    if (offset) {
        *offset = vjs->len;
    }

    vjs->len += size;

    return ret_ok;
}
