#include <stdio.h>
#include <stdint.h>
#include "commercial_modes.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_commercial_modes_t cms;

    pmidx_t offset;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_commercial_modes_init (&cms, &pool);

    printf("now: %d %d\n", cms.len, cms.size);

    {
        const char *id = "OP";
        const char *name = "Opstapper";
        tdata_commercial_modes_add (&cms, &id, &name, 1, &offset);
    }

    printf("now: %d %d %d\n", cms.len, cms.size, offset);

    printf("%s %s\n", &cms.pool->pool[cms.commercial_mode_ids[0]], &cms.pool->pool[cms.commercial_mode_names[0]]);

    tdata_commercial_modes_mrproper (&cms);
    tdata_string_pool_mrproper (&pool);

    return 0;
}
