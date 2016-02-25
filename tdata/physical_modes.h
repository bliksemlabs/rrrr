#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"

typedef struct {
    uint32_t *physical_mode_ids;
    uint32_t *physical_mode_names;

    tdata_string_pool_t *pool;

    pmidx_t size; /* Total amount of memory */
    pmidx_t len;  /* Length of the list   */
} tdata_physical_modes_t;

ret_t tdata_physical_modes_init (tdata_physical_modes_t *pms, tdata_string_pool_t *pool);
ret_t tdata_physical_modes_fake (tdata_physical_modes_t *pms, const uint32_t *physical_mode_ids, const uint32_t *physical_mode_names, const uint32_t len);
ret_t tdata_physical_modes_mrproper (tdata_physical_modes_t *pms);
ret_t tdata_physical_modes_ensure_size (tdata_physical_modes_t *pms, pmidx_t size);
ret_t tdata_physical_modes_add (tdata_physical_modes_t *pms,
                           const char **id,
                           const char **names,
                           const pmidx_t size,
                           pmidx_t *offset);
