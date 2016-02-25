#include <stdio.h>
#include <stdint.h>
#include "journey_pattern_points.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_transfers_t transfers;
    tdata_stop_areas_t sas;
    tdata_stop_points_t sps;
    tdata_journey_pattern_points_t jpps;

    spidx_t stop_point_offset;
    jppidx_t jpp_offset;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_transfers_init (&transfers);
    tdata_stop_areas_init (&sas, &pool);
    tdata_stop_points_init (&sps, &sas, &transfers, &pool);
    tdata_journey_pattern_points_init (&jpps, &sps, &pool);

    {
        const char *id = "12";
        const char *name = "Test Area";
        const latlon_t ll = {0.0, 0.0};
        const char *timezone = "Europe/Amsterdam";
        tdata_stop_areas_add (&sas, &id, &name, &ll, &timezone, 1, NULL);
    }

    {
        const char *id[] = {"1", "2"};
        const char *name[] = {"Stop 1", "Stop 2"};
        const char *platformcode[] = {"A", ""};
        const latlon_t ll[] = {{0.0, 0.0}, {1.0, 1.0}};
        const rtime_t waittime[] = {0, 0};
        const spidx_t stoparea[] = {0, 0};
        const uint8_t attributes[] = {0, 0};
        tdata_stop_points_add (&sps, id, name, platformcode, ll, waittime, stoparea, attributes, 2, &stop_point_offset);
    }

    printf("now: %d %d\n", sps.len, sps.size);

    {
        const char *headsign = "Going somewhere";
        const uint8_t attributes = 0;
        tdata_journey_pattern_points_add (&jpps, &stop_point_offset, &headsign, &attributes, 1, NULL);

        stop_point_offset++;
        tdata_journey_pattern_points_add (&jpps, &stop_point_offset, &headsign, &attributes, 1, &jpp_offset);
    }

    printf("now: %d %d %d\n", jpps.len, jpps.size, jpp_offset);

    tdata_journey_pattern_points_mrproper (&jpps);
    tdata_transfers_mrproper (&transfers);
    tdata_stop_areas_mrproper (&sas);
    tdata_stop_points_mrproper (&sps);
    tdata_string_pool_mrproper (&pool);

    return 0;
}
