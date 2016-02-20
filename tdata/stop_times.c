#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "stop_times.h"

#define REALLOC_EXTRA_SIZE     1024

ret_t
tdata_stop_times_init (tdata_stop_times_t *sts)
{
    sts->arrival = NULL;
    sts->departure = NULL;

    sts->size = 0;
    sts->len = 0;

    return ret_ok;
}

ret_t
tdata_stop_times_mrproper (tdata_stop_times_t *sts)
{
    if (unlikely (sts->size == 0 && sts->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (sts->arrival)
        free (sts->arrival);
 
    if (sts->departure)
        free (sts->departure);
    
    return tdata_stop_times_init (sts);
}

ret_t
tdata_stop_times_ensure_size (tdata_stop_times_t *sts, uint32_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= sts->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (sts->arrival == NULL) {
        sts->arrival   = (rtime_t *) calloc (size, sizeof(rtime_t));
        sts->departure = (rtime_t *) calloc (size, sizeof(rtime_t));
        
        if (unlikely (sts->arrival == NULL || sts->departure == NULL)) {
            return ret_nomem;
        }
        sts->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (sts->arrival, sizeof(rtime_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sts->arrival = (rtime_t *) p;

    p = realloc (sts->departure, sizeof(rtime_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    sts->departure = (rtime_t *) p;

    sts->size = size;
    
    return ret_ok; 
}

ret_t
tdata_stop_times_add (tdata_stop_times_t *sts,
                     const rtime_t *arrival,
                     const rtime_t *departure,
                     const uint32_t size,
                     uint32_t *offset)
{
    long available;

    if (unlikely (sts->size == 0 && sts->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = sts->size - sts->len;

    if ((uint32_t) available < size) {
        if (unlikely (tdata_stop_times_ensure_size (sts, (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (sts->arrival + sts->len, arrival, sizeof(rtime_t) * size);
    memcpy (sts->departure + sts->len, departure, sizeof(rtime_t) * size);

    if (offset) {
        *offset = sts->len;
    }    

    sts->len += size;

    return ret_ok;
}
