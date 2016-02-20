#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"

typedef struct {
    uint32_t *stop_area_ids;
    latlon_t *stop_area_coords;
    uint32_t *stop_area_nameidx;
    uint32_t  *stop_area_timezones;

    tdata_string_pool_t *pool;

    spidx_t size; /* Total amount of memory */
    spidx_t len;  /* Length of the list   */
} tdata_stop_areas_t;

ret_t tdata_stop_areas_init (tdata_stop_areas_t *sas, tdata_string_pool_t *pool);
ret_t tdata_stop_areas_mrproper (tdata_stop_areas_t *sas);
ret_t tdata_stop_areas_ensure_size (tdata_stop_areas_t *sas, spidx_t size);
ret_t tdata_stop_areas_add (tdata_stop_areas_t *sas,
                            const char **id,
                            const char **name,
                            const latlon_t *coord,
                            const char **timezone,
                            const spidx_t size,
                            spidx_t *offset);
