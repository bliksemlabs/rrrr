#include <stdio.h>
#include <stdint.h>
#include "operators.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_operators_t ops;
    
    opidx_t op;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_operators_init (&ops, &pool);
    
    printf("now: %d %d\n", ops.len, ops.size);

    { 
        const char *id = "cxx";
        const char *url = "http://connexxion.nl/";
        const char *name = "Connexxion";
        tdata_operators_add (&ops, &id, &url, &name, 1, NULL);
    }

    { 
        const char *id = "arr";
        const char *url = "http://arriva.nl/";
        const char *name = "Arriva";
        tdata_operators_add (&ops, &id, &url, &name, 1, &op);
    }
 
    printf("now: %d %d %d\n", ops.len, ops.size, op);
    
    printf("%s %s %s\n", &ops.pool->pool[ops.operator_ids[0]], &ops.pool->pool[ops.operator_urls[0]], &ops.pool->pool[ops.operator_names[0]]);
    printf("%s %s %s\n", &ops.pool->pool[ops.operator_ids[1]], &ops.pool->pool[ops.operator_urls[1]], &ops.pool->pool[ops.operator_names[1]]);

    tdata_operators_mrproper (&ops);
    tdata_string_pool_mrproper (&pool);
}
