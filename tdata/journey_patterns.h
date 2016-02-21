#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"
#include "commercial_modes.h"
#include "routes.h"
#include "journey_pattern_points.h"
#include "vehicle_journeys.h"

/* Figure out if having the tdata_journey_pattrns_t as a struct itself,
 * instead of a list performs better due to locality queries.
 */

typedef struct {
    uint32_t *journey_pattern_point_offset;
    vjidx_t  *vj_index;
    jppidx_t *n_stops;
    jp_vjoffset_t *n_vjs;
    uint16_t *attributes;
    routeidx_t *route_index;
    cmidx_t *commercial_mode_for_jp;

    tdata_commercial_modes_t *cms;
    tdata_routes_t *routes;
    tdata_journey_pattern_points_t *jpps;
    tdata_vehicle_journeys_t *vjs;

    jpidx_t size; /* Total amount of memory */
    jpidx_t len;  /* Length of the list   */
} tdata_journey_patterns_t;

ret_t tdata_journey_patterns_init (tdata_journey_patterns_t *jps, tdata_commercial_modes_t *cms, tdata_routes_t *routes, tdata_journey_pattern_points_t *jpps, tdata_vehicle_journeys_t *vjs);
ret_t tdata_journey_patterns_mrproper (tdata_journey_patterns_t *jps);
ret_t tdata_journey_patterns_ensure_size (tdata_journey_patterns_t *jps, jpidx_t size);
ret_t tdata_journey_patterns_add (tdata_journey_patterns_t *jps,
                            const uint32_t **journey_pattern_point_offset,
                            const vjidx_t **vj_index,
                            const jppidx_t *n_stops,
                            const jp_vjoffset_t *n_vjs,
                            const uint16_t *attributes,
                            const routeidx_t *route_index,
                            const cmidx_t *commercial_mode_for_jp,
                            const jpidx_t size,
                            jpidx_t *offset);
