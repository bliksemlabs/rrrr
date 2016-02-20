#include "common.h"
#include "tdata_common.h"

typedef struct {
    vehicle_journey_ref_t *vehicle_journey_transfers_forward;
    vehicle_journey_ref_t *vehicle_journey_transfers_backward;
    
    uint32_t size; /* Total amount of memory */
    uint32_t len;  /* Length of the list   */
} tdata_vehicle_transfers_t;

ret_t tdata_vehicle_transfers_init (tdata_vehicle_transfers_t *vt);
ret_t tdata_vehicle_transfers_mrproper (tdata_vehicle_transfers_t *vt);
ret_t tdata_vehicle_transfers_ensure_size (tdata_vehicle_transfers_t *vt, uint32_t size);
ret_t tdata_vehicle_transfers_add (tdata_vehicle_transfers_t *vt,
                           const vehicle_journey_ref_t* vehicle_journey_transfers_forward,
                           const vehicle_journey_ref_t* vehicle_journey_transfers_backward,
                           const uint32_t size,
                           uint32_t *offset);
