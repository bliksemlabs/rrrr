#include <stdio.h>
#include <stdint.h>
#include "string_pool.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    uint32_t idx;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);

    printf("now: %d %d\n", pool.len, pool.size);

    tdata_string_pool_add (&pool, "ABC", 4, &idx);

    printf("%d %s\n", idx, &pool.pool[idx]);

    printf("now: %d %d\n", pool.len, pool.size);

    tdata_string_pool_add (&pool, "DEFG", 5, &idx);

    printf("%d %s\n", idx, &pool.pool[idx]);

    printf("now: %d %d\n", pool.len, pool.size);

    tdata_string_pool_ensure_size (&pool, 1000);

    printf("now: %d %d\n", pool.len, pool.size);

    tdata_string_pool_add (&pool, "HI", 3, &idx);

    printf("%d %s\n", idx, &pool.pool[idx]);

    printf("%d %s\n", 0, &pool.pool[0]);

    tdata_string_pool_mrproper (&pool);

    return 0;
}
