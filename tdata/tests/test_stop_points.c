#include <stdio.h>
#include <stdint.h>
#include "stop_points.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_transfers_t transfers;
    tdata_stop_areas_t sas;
    tdata_stop_points_t sps;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_transfers_init (&transfers);
    tdata_stop_areas_init (&sas, &pool);
    tdata_stop_points_init (&sps, &sas, &transfers, &pool);

    {
        const char *id = "12";
        const char *name = "Test Area";
        const latlon_t ll = {0.0, 0.0};
        const char *timezone = "Europe/Amsterdam";
        tdata_stop_areas_add (&sas, &id, &name, &ll, &timezone, 1, NULL);
    }

    printf("now: %d %d\n", sps.len, sps.size);

    {
        const char *id = "12";
        const char *name = "Test1234";
        const char *platformcode = "AB";
        const latlon_t ll = {0.0, 0.0};
        const rtime_t waittime = 2;
        const spidx_t stoparea = 0;
        const uint8_t attributes = 0;
        tdata_stop_points_add (&sps, &id, &name, &platformcode, &ll, &waittime, &stoparea, &attributes, 1, NULL);
    }

    printf("now: %d %d\n", sps.len, sps.size);

    printf("%s %s %s\n", &sps.pool->pool[sps.platformcodes[0]], &sps.pool->pool[sps.stop_point_ids[0]], &sps.pool->pool[sps.stop_point_nameidx[0]]);

    tdata_transfers_mrproper (&transfers);
    tdata_stop_areas_mrproper (&sas);
    tdata_stop_points_mrproper (&sps);
    tdata_string_pool_mrproper (&pool);

    return 0;
}
