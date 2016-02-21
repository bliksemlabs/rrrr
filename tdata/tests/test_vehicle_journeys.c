#include <stdio.h>
#include <stdint.h>
#include "vehicle_journeys.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_stop_times_t sps;
    tdata_vehicle_transfers_t vts;
    tdata_vehicle_journeys_t vjs;

    uint32_t st_offset;
    uint32_t vt_offset;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_stop_times_init (&sps);
    tdata_vehicle_transfers_init (&vts);
    tdata_vehicle_journeys_init (&vjs, &vts, &sps, &pool);

    {
        const rtime_t arrival[] = {0, 15, 30};
        const rtime_t departure[] = {0, 20, 30};
        tdata_stop_times_add (&sps, arrival, departure, 3, &st_offset);
    }

    {
        const vehicle_journey_ref_t vehicle_journey_transfers[] = {{0, 1}, {0, 0}};
        tdata_vehicle_transfers_add (&vts, vehicle_journey_transfers, 1, &vt_offset);
    }

    {
        const char *id[] = {"Trip 1", "Trip 2"};
        const rtime_t begin_time[] = {0, 30};
        const vj_attribute_mask_t vj_attributes[] = {0, 0};
        const calendar_t vj_active[] = {1, 1};
        const int8_t vj_time_offsets[] = {0, 0};
        const vehicle_journey_ref_t vehicle_journey_transfers_forward[] = {{0, 1}, {0,0}};
        const vehicle_journey_ref_t vehicle_journey_transfers_backward[] = {{0, 0}, {0, 0}};
        const uint32_t vj_transfers_forward_offset[] = {0, 0};
        const uint32_t vj_transfers_backward_offset[] = {0, 1};
        const uint8_t n_transfers_forward[] = {1, 0};
        const uint8_t n_transfers_backward[] = {0, 1};
        tdata_vehicle_journeys_add (&vjs, id, &st_offset, begin_time, vj_attributes, vj_active, vj_time_offsets, vehicle_journey_transfers_forward, vehicle_journey_transfers_backward, vj_transfers_forward_offset, vj_transfers_backward_offset, n_transfers_forward, n_transfers_backward, 2, NULL);
    }

    tdata_vehicle_journeys_mrproper (&vjs);
    tdata_vehicle_transfers_mrproper (&vts);
    tdata_stop_times_mrproper (&sps);
    tdata_string_pool_mrproper (&pool);

    return 0;
}

