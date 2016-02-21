#include <stdio.h>
#include <stdint.h>
#include "journey_patterns.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;

    tdata_transfers_t transfers;
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

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);

    tdata_transfers_init (&transfers);
    tdata_stop_points_init (&sps, &transfers, &pool);
    tdata_journey_pattern_points_init (&jpps, &sps, &pool);

    tdata_stop_times_init (&sts);
    tdata_vehicle_transfers_init (&vts);
    tdata_vehicle_journeys_init (&vjs, &vts, &sts, &pool);

    tdata_operators_init (&ops, &pool);
    tdata_physical_modes_init (&pms, &pool);
    tdata_lines_init (&lines, &ops, &pms, &pool);
    tdata_routes_init (&routes, &lines);

    tdata_commercial_modes_init (&cms, &pool);

    tdata_journey_patterns_init (&jps, &cms, &routes, &jpps, &vjs);

    tdata_journey_patterns_ensure_size (&jps, 1);

    tdata_journey_patterns_mrproper (&jps);

    tdata_commercial_modes_mrproper (&cms);

    tdata_lines_mrproper (&lines);
    tdata_physical_modes_mrproper (&pms);
    tdata_operators_mrproper (&ops);

    tdata_vehicle_journeys_mrproper (&vjs);
    tdata_vehicle_transfers_mrproper (&vts);
    tdata_stop_times_mrproper (&sts);

    tdata_journey_pattern_points_mrproper (&jpps);
    tdata_transfers_mrproper (&transfers);
    tdata_stop_points_mrproper (&sps);

    tdata_string_pool_mrproper (&pool);

    return 0;
}
