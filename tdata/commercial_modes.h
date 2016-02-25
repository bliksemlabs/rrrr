#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"

typedef struct {
    uint32_t *commercial_mode_ids;
    uint32_t *commercial_mode_names;

    tdata_string_pool_t *pool;

    cmidx_t size; /* Total amount of memory */
    cmidx_t len;  /* Length of the list   */
} tdata_commercial_modes_t;

ret_t tdata_commercial_modes_init (tdata_commercial_modes_t *cm, tdata_string_pool_t *pool);
ret_t tdata_commercial_modes_fake (tdata_commercial_modes_t *cms, const uint32_t *commercial_mode_ids, const uint32_t *commercial_mode_names, const uint32_t len);
ret_t tdata_commercial_modes_mrproper (tdata_commercial_modes_t *cm);
ret_t tdata_commercial_modes_ensure_size (tdata_commercial_modes_t *cm, cmidx_t size);
ret_t tdata_commercial_modes_add (tdata_commercial_modes_t *cm,
                           const char **id,
                           const char **names,
                           const cmidx_t size,
                           cmidx_t *offset);
