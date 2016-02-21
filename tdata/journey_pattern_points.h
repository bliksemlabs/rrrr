#include "common.h"
#include "tdata_common.h"
#include "stop_points.h"

typedef struct {
    spidx_t *journey_pattern_points;
    uint32_t *journey_pattern_point_headsigns;
    uint8_t *journey_pattern_point_attributes;

    tdata_stop_points_t *sps;
    tdata_string_pool_t *pool;

    jppidx_t size; /* Total amount of memory */
    jppidx_t len;  /* Length of the list   */
} tdata_journey_pattern_points_t;

ret_t tdata_journey_pattern_points_init (tdata_journey_pattern_points_t *jpps, tdata_stop_points_t *sps, tdata_string_pool_t *pool);
ret_t tdata_journey_pattern_points_mrproper (tdata_journey_pattern_points_t *jpps);
ret_t tdata_journey_pattern_points_ensure_size (tdata_journey_pattern_points_t *jpps, jppidx_t size);
ret_t tdata_journey_pattern_points_add (tdata_journey_pattern_points_t *jpps,
                            const spidx_t *journey_pattern_points,
                            const char **journey_pattern_point_headsigns,
                            const uint8_t *journey_pattern_point_attributes,
                            const jppidx_t size,
                            jppidx_t *offset);
