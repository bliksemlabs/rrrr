#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "transfers.h"

#define REALLOC_EXTRA_SIZE     64

ret_t
tdata_transfers_init (tdata_transfers_t *transfers)
{
    transfers->transfer_target_stops = NULL;
    transfers->transfer_durations = NULL;

    transfers->size = 0;
    transfers->len = 0;

    return ret_ok;
}

ret_t
tdata_transfers_fake (tdata_transfers_t *transfers, const spidx_t *transfer_target_stops, const rtime_t *transfer_durations, const uint32_t len) {
    transfers->transfer_target_stops = (spidx_t *) transfer_target_stops;
    transfers->transfer_durations = (rtime_t *) transfer_durations;

    transfers->size = 0;
    transfers->len = len;

    return ret_ok;
}

ret_t
tdata_transfers_mrproper (tdata_transfers_t *transfers)
{
    if (unlikely (transfers->size == 0 && transfers->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (transfers->transfer_target_stops)
        free (transfers->transfer_target_stops);

    if (transfers->transfer_durations)
        free (transfers->transfer_durations);

    return tdata_transfers_init (transfers);
}

ret_t
tdata_transfers_ensure_size (tdata_transfers_t *transfers, uint32_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= transfers->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (transfers->transfer_target_stops == NULL) {
        transfers->transfer_target_stops = (spidx_t *) calloc (size + 1, sizeof(spidx_t));
        transfers->transfer_durations    = (rtime_t *) calloc (size + 1, sizeof(rtime_t));

        if (unlikely (transfers->transfer_target_stops == NULL ||
                      transfers->transfer_durations == NULL)) {
            return ret_nomem;
        }
        transfers->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (transfers->transfer_target_stops, sizeof(spidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    transfers->transfer_target_stops = (spidx_t *) p;

    p = realloc (transfers->transfer_durations, sizeof(rtime_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    transfers->transfer_durations= (rtime_t *) p;

    transfers->size = size;

    return ret_ok;
}

ret_t
tdata_transfers_add (tdata_transfers_t *transfers,
                     const spidx_t *transfer_target_stops,
                     const rtime_t *transfer_durations,
                     const uint32_t size,
                     uint32_t *offset)
{
    long available;

    if (unlikely (transfers->size == 0 && transfers->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = transfers->size - transfers->len;

    if ((uint32_t) available < size) {
        if (unlikely (tdata_transfers_ensure_size (transfers, transfers->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (transfers->transfer_target_stops + transfers->len, transfer_target_stops, sizeof(spidx_t) * size);
    memcpy (transfers->transfer_durations + transfers->len, transfer_durations, sizeof(rtime_t) * size);

    if (offset) {
        *offset = transfers->len;
    }

    transfers->len += size;

    return ret_ok;
}
