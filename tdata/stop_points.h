#include "common.h"
#include "tdata_common.h"
#include "string_pool.h"
#include "transfers.h"
#include "stop_areas.h"

typedef struct {
    uint32_t *platformcodes;
    uint32_t *stop_point_ids;
    latlon_t *stop_point_coords;
    uint32_t *stop_point_nameidx;
    rtime_t  *stop_point_waittime;
    uint8_t  *stop_point_attributes;
    spidx_t  *stop_area_for_stop_point;
    uint32_t *transfers_offset;

    tdata_stop_areas_t *sas;
    tdata_transfers_t *transfers;
    tdata_string_pool_t *pool;

    spidx_t size; /* Total amount of memory */
    spidx_t len;  /* Length of the list   */
} tdata_stop_points_t;

ret_t tdata_stop_points_init (tdata_stop_points_t *sps, tdata_stop_areas_t *sas, tdata_transfers_t *transfers, tdata_string_pool_t *pool);
ret_t tdata_stop_points_fake (tdata_stop_points_t *sps, const uint32_t *platformcodes, const uint32_t *stop_point_ids, const latlon_t *stop_point_coords, const uint32_t *stop_point_nameidx, const rtime_t *stop_point_waittime, const uint8_t *stop_point_attributes, const spidx_t *stop_area_for_stop_point, const uint32_t *transfers_offset, const uint32_t len);
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
