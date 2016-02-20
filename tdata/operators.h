#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"

typedef struct {
    uint32_t *operator_ids;
    uint32_t *operator_urls;
    uint32_t *operator_names;
    
    tdata_string_pool_t *pool;

    opidx_t size; /* Total amount of memory */
    opidx_t len;  /* Length of the list   */
} tdata_operators_t;

ret_t tdata_operators_init (tdata_operators_t *ops, tdata_string_pool_t *pool);
ret_t tdata_operators_mrproper (tdata_operators_t *ops);
ret_t tdata_operators_ensure_size (tdata_operators_t *ops, opidx_t size);
ret_t tdata_operators_add (tdata_operators_t *ops,
                           const char **id,
                           const char **urls,
                           const char **names,
                           const opidx_t size,
                           opidx_t *offset);
