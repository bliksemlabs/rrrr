#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "container.h"

ret_t
tdata_container_init (tdata_container_t *container)
{
    container->calendar_start_time = 0;
    container->n_days = 0;
    container->timezone = 0;
    container->utc_offset = 0;

    tdata_string_pool_init (&container->string_pool);

    tdata_transfers_init (&container->transfers);
    tdata_stop_areas_init (&container->sas, &container->string_pool);
    tdata_stop_points_init (&container->sps, &container->sas, &container->transfers, &container->string_pool);
    tdata_journey_pattern_points_init (&container->jpps, &container->sps, &container->string_pool);

    tdata_stop_times_init (&container->sts);
    tdata_vehicle_transfers_init (&container->vts);
    tdata_vehicle_journeys_init (&container->vjs, &container->vts, &container->sts, &container->string_pool);

    tdata_operators_init (&container->ops, &container->string_pool);
    tdata_physical_modes_init (&container->pms, &container->string_pool);
    tdata_lines_init (&container->lines, &container->ops, &container->pms, &container->string_pool);
    tdata_routes_init (&container->routes, &container->lines);

    tdata_commercial_modes_init (&container->cms, &container->string_pool);

    tdata_journey_patterns_init (&container->jps, &container->cms,
                                 &container->routes, &container->jpps,
                                 &container->vjs);

    return ret_ok;
}

ret_t
tdata_container_mrproper (tdata_container_t *container)
{
    tdata_journey_patterns_mrproper (&container->jps);

    tdata_commercial_modes_mrproper (&container->cms);

    tdata_routes_mrproper (&container->routes);
    tdata_lines_mrproper (&container->lines);
    tdata_physical_modes_mrproper (&container->pms);
    tdata_operators_mrproper (&container->ops);

    tdata_vehicle_journeys_mrproper (&container->vjs);
    tdata_vehicle_transfers_mrproper (&container->vts);
    tdata_stop_times_mrproper (&container->sts);

    tdata_journey_pattern_points_mrproper (&container->jpps);
    tdata_transfers_mrproper (&container->transfers);
    tdata_stop_areas_mrproper (&container->sas);
    tdata_stop_points_mrproper (&container->sps);

    tdata_string_pool_mrproper (&container->string_pool);

    return tdata_container_init (container);
}

