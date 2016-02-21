#include <stdio.h>
#include <stdint.h>
#include "physical_modes.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_physical_modes_t pms;

    pmidx_t offset;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_physical_modes_init (&pms, &pool);

    printf("now: %d %d\n", pms.len, pms.size);

    {
        const char *id[] = { "BUS", "TRAIN", "TRAM" };
        const char *name[] = { "Bus", "Train", "Tram" };
        tdata_physical_modes_add (&pms, id, name, 2, &offset);
    }

    {
        const char *id[] = { "TRAM" };
        const char *name[] = { "Tram" };
        tdata_physical_modes_add (&pms, id, name, 1, &offset);
    }

    printf("now: %d %d %d\n", pms.len, pms.size, offset);

    printf("%s %s\n", &pms.pool->pool[pms.physical_mode_ids[0]], &pms.pool->pool[pms.physical_mode_names[0]]);
    printf("%s %s\n", &pms.pool->pool[pms.physical_mode_ids[1]], &pms.pool->pool[pms.physical_mode_names[1]]);
    printf("%s %s\n", &pms.pool->pool[pms.physical_mode_ids[2]], &pms.pool->pool[pms.physical_mode_names[2]]);

    tdata_physical_modes_mrproper (&pms);
    tdata_string_pool_mrproper (&pool);

    return 0;
}
