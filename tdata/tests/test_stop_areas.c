#include <stdio.h>
#include <stdint.h>
#include "stop_areas.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_stop_areas_t sas;

    spidx_t offset;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_stop_areas_init (&sas, &pool);

    printf("now: %d %d\n", sas.len, sas.size);

    {
        const char *id[] = {"12", "13"};
        const char *name[] = {"SA1", "SA2"};
        const latlon_t ll[] = {{0.0, 0.0}, {1.0, 0.0}};
        const char *time_zone[] = {"Europe/Amsterdam", "Europe/Amsterdam"};
        tdata_stop_areas_add (&sas, id, name, ll, time_zone, 2, &offset);
    }

    printf("now: %d %d %d\n", sas.len, sas.size, offset);

    printf("%s %s %s\n", &sas.pool->pool[sas.stop_area_ids[0]], &sas.pool->pool[sas.stop_area_nameidx[0]], &sas.pool->pool[sas.stop_area_timezones[0]]);
    printf("%s %s %s\n", &sas.pool->pool[sas.stop_area_ids[1]], &sas.pool->pool[sas.stop_area_nameidx[1]], &sas.pool->pool[sas.stop_area_timezones[1]]);

    tdata_stop_areas_mrproper (&sas);
    tdata_string_pool_mrproper (&pool);

    return 0;
}
