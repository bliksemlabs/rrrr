#include "common.h"
#include "tdata_common.h"

/* todo check performance vs struct stoptime_t { rtime_t arrival; rtime_t departure; }; */

typedef struct {
    rtime_t *arrival;
    rtime_t *departure;

    uint32_t size; /* Total amount of memory */
    uint32_t len;  /* Length of the list   */
} tdata_stop_times_t;

ret_t tdata_stop_times_init (tdata_stop_times_t *sts);
ret_t tdata_stop_times_mrproper (tdata_stop_times_t *sts);
ret_t tdata_stop_times_ensure_size (tdata_stop_times_t *sts, uint32_t size);
ret_t tdata_stop_times_add (tdata_stop_times_t *sts,
                           const rtime_t *arrival,
                           const rtime_t *departure,
                           const uint32_t size,
                           uint32_t *offset);
