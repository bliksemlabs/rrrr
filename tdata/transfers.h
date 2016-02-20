#include "common.h"
#include "tdata_common.h"

typedef struct {
    spidx_t *transfer_target_stops;
    rtime_t *transfer_durations;
    
    uint32_t size; /* Total amount of memory */
    uint32_t len;  /* Length of the list   */
} tdata_transfers_t;

ret_t tdata_transfers_init (tdata_transfers_t *transfers);
ret_t tdata_transfers_mrproper (tdata_transfers_t *transfers);
ret_t tdata_transfers_ensure_size (tdata_transfers_t *transfers, uint32_t size);
ret_t tdata_transfers_add (tdata_transfers_t *transfers,
                           const spidx_t *transfer_target_stops,
                           const rtime_t *transfer_durations,
                           const uint32_t size,
                           uint32_t *offset);
