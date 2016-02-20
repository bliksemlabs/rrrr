#include <stdio.h>
#include <stdint.h>
#include "stop_points.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_stop_points_t sps;

    tdata_string_pool_init (&pool);
    tdata_stop_points_init (&sps, &pool);
    
    printf("now: %d %d\n", sps.len, sps.size);

    { 
        const char *id = "12";
        const char *name = "Test1234";
        const char *platformcode = "AB";
        const latlon_t ll = {0.0, 0.0};
        const rtime_t waittime = 2;
        const spidx_t stoparea = 3;
        const uint8_t attributes = 0; 
        tdata_stop_points_add (&sps, &id, &name, &platformcode, &ll, &waittime, &stoparea, &attributes, 1, NULL);
    }
    
    printf("now: %d %d\n", sps.len, sps.size);
    
    printf("%s %s %s\n", &sps.pool->pool[sps.platformcodes[0]], &sps.pool->pool[sps.stop_point_ids[0]], &sps.pool->pool[sps.stop_point_nameidx[0]]);

    tdata_stop_points_mrproper (&sps);
    tdata_string_pool_mrproper (&pool);
}
