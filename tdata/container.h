#include "common.h"
#include "tdata_common.h"
#include "journey_patterns.h"

typedef struct {
    uint64_t calendar_start_time;
    uint32_t n_days;
    uint32_t timezone;
    int32_t utc_offset;

    tdata_string_pool_t string_pool;
    tdata_transfers_t transfers;
    tdata_stop_areas_t sas;
    tdata_stop_points_t sps;
    tdata_journey_pattern_points_t jpps;

    tdata_stop_times_t sts;
    tdata_vehicle_transfers_t vts;
    tdata_vehicle_journeys_t vjs;

    tdata_operators_t ops;
    tdata_physical_modes_t pms;
    tdata_lines_t lines;
    tdata_routes_t routes;

    tdata_commercial_modes_t cms;
    tdata_journey_patterns_t jps;

} tdata_container_t;

ret_t tdata_container_init (tdata_container_t *container);
ret_t tdata_container_mrproper (tdata_container_t *container);
