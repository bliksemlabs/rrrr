#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"
#include "stop_times.h"
#include "vehicle_transfers.h"

typedef struct {
    uint32_t *stop_times_offset;
    rtime_t *begin_time;
    vj_attribute_mask_t *vj_attributes;

    uint32_t *vj_ids;
    calendar_t *vj_active;
    int8_t *vj_time_offsets;
    vehicle_journey_ref_t *vehicle_journey_transfers_forward;
    vehicle_journey_ref_t *vehicle_journey_transfers_backward;

    uint32_t *vj_transfers_forward_offset;
    uint32_t *vj_transfers_backward_offset;

    /* Maybe we could do something like the lowest 4 bits, and highest 4 bits? */
    uint8_t *n_transfers_forward;
    uint8_t *n_transfers_backward;

    tdata_vehicle_transfers_t *vts;
    tdata_stop_times_t *sts;
    tdata_string_pool_t *pool;

    spidx_t size; /* Total amount of memory */
    spidx_t len;  /* Length of the list   */
} tdata_vehicle_journeys_t;

ret_t tdata_vehicle_journeys_init (tdata_vehicle_journeys_t *vjs, tdata_vehicle_transfers_t *vts, tdata_stop_times_t *sts,  tdata_string_pool_t *pool);
ret_t tdata_vehicle_journeys_mrproper (tdata_vehicle_journeys_t *vjs);
ret_t tdata_vehicle_journeys_ensure_size (tdata_vehicle_journeys_t *vjs, vjidx_t size);
ret_t tdata_vehicle_journeys_add (tdata_vehicle_journeys_t *vjs,
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
                             vjidx_t *offset);
