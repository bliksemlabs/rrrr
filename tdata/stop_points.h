#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"

typedef struct {
    uint32_t *platformcodes;
    uint32_t *stop_point_ids;
    latlon_t *stop_point_coords;
    uint32_t *stop_point_nameidx;
    rtime_t  *stop_point_waittime;
    uint8_t  *stop_point_attributes;
    spidx_t  *stop_area_for_stop_point;
    uint32_t *journey_patterns_at_stop_point_offset;
    uint32_t *transfers_offset;

    tdata_string_pool_t *pool;

    spidx_t size; /* Total amount of memory */
    spidx_t len;  /* Length of the list   */
} tdata_stop_points_t;

ret_t tdata_stop_points_init (tdata_stop_points_t *sps, tdata_string_pool_t *pool);
ret_t tdata_stop_points_mrproper (tdata_stop_points_t *sps);
ret_t tdata_stop_points_ensure_size (tdata_stop_points_t *sps, spidx_t size);
ret_t tdata_stop_points_add (tdata_stop_points_t *sps,
                             const char **id,
                             const char **name,
                             const char **platformcode,
                             const latlon_t *coord,
                             const rtime_t *waittime,
                             const spidx_t *stop_area,
                             const uint8_t *attributes,
                             const spidx_t size,
                             spidx_t *offset);
