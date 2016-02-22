#include "common.h"
#include "tdata_common.h"
#include "container.h"

typedef struct {
    tdata_container_t container;

    void *base;
    size_t size;
} tdata_container_mmap_t;

ret_t tdata_container_mmap_init (tdata_container_mmap_t *container, const char *filename);
ret_t tdata_container_mmap_mrproper (tdata_container_mmap_t *container);
