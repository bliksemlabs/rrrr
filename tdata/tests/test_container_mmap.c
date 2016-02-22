#include <stdio.h>
#include <stdint.h>
#include "container_mmap.h"

int main(int argv, char *args[]) {
    tdata_container_mmap_t container;

    if (argv < 2) return -1;

    tdata_container_mmap_init (&container, args[1]);

    printf("%d %s\n", 0, &container.container.string_pool.pool[0]);

    tdata_container_mmap_mrproper (&container);

    return 0;
}

