#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "vehicle_transfers.h"

#define REALLOC_EXTRA_SIZE     64

ret_t
tdata_vehicle_transfers_init (tdata_vehicle_transfers_t *vt)
{
    vt->vehicle_journey_transfers = NULL;

    vt->size = 0;
    vt->len = 0;

    return ret_ok;
}

ret_t
tdata_vehicle_transfers_mrproper (tdata_vehicle_transfers_t *vt)
{
    if (unlikely (vt->size == 0 && vt->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (vt->vehicle_journey_transfers)
        free (vt->vehicle_journey_transfers);

    return tdata_vehicle_transfers_init (vt);
}

ret_t
tdata_vehicle_transfers_ensure_size (tdata_vehicle_transfers_t *vt, uint32_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= vt->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (vt->vehicle_journey_transfers == NULL) {
        vt->vehicle_journey_transfers = (vehicle_journey_ref_t *) calloc (size, sizeof(vehicle_journey_ref_t));

        if (unlikely (vt->vehicle_journey_transfers == NULL)) {
            return ret_nomem;
        }
        vt->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (vt->vehicle_journey_transfers, sizeof(vehicle_journey_ref_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    vt->vehicle_journey_transfers = (vehicle_journey_ref_t *) p;

    vt->size = size;

    return ret_ok;
}

ret_t
tdata_vehicle_transfers_add (tdata_vehicle_transfers_t *vt,
                     const vehicle_journey_ref_t *vehicle_journey_transfers,
                     const uint32_t size,
                     uint32_t *offset)
{
    long available;

    if (unlikely (vt->size == 0 && vt->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = vt->size - vt->len;

    if ((uint32_t) available < size) {
        if (unlikely (tdata_vehicle_transfers_ensure_size (vt, vt->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (vt->vehicle_journey_transfers + vt->len, vehicle_journey_transfers, sizeof(vehicle_journey_ref_t) * size);

    if (offset) {
        *offset = vt->len;
    }

    vt->len += size;

    return ret_ok;
}
