#include "common.h"
#include "tdata_common.h"
#include "journey_patterns.h"

/* Figure out if having the tdata_journey_pattrns_t as a struct itself,
 * instead of a list performs better due to locality queries.
 */

typedef struct {
    tdata_journey_patterns_t jps;

    /* indices */
    rtime_t *min_time;
    rtime_t *max_time;
    calendar_t *journey_pattern_active;
    jpidx_t *journey_patterns_at_stop;
    /* journey_patterns_at_stop_point_offset */

} tdata_journey_patterns_indexed_t;

ret_t tdata_journey_patterns_indexed_init (tdata_journey_patterns_indexed_t *jps);
ret_t tdata_journey_patterns_indexed_mrproper (tdata_journey_patterns_indexed_t *jps);
ret_t tdata_journey_patterns_indexed_ensure_size (tdata_journey_patterns_indexed_t *jps, jpidx_t size);
ret_t tdata_journey_patterns_indexed_add (tdata_journey_patterns_indexed_t *jps,
                            const uint32_t **journey_pattern_point_offset,
                            const vjidx_t **vj_index,
                            const jppidx_t *n_stops,
                            const jp_vjoffset_t *n_vjs,
                            const uint16_t *attributes,
                            const routeidx_t *route_index,
                            const spidx_t size,
                            jpidx_t *offset);
