#include "common.h"
#include "tdata_common.h"

#include "lines.h"

typedef struct {
    lineidx_t *line_for_route;

    tdata_lines_t *lines;

    routeidx_t size; /* Total amount of memory */
    routeidx_t len;  /* Length of the list   */
} tdata_routes_t;

ret_t tdata_routes_init (tdata_routes_t *routes, tdata_lines_t *lines);
ret_t tdata_routes_mrproper (tdata_routes_t *routes);
ret_t tdata_routes_ensure_size (tdata_routes_t *routes, routeidx_t size);
ret_t tdata_routes_add (tdata_routes_t *routes,
                           const lineidx_t *line_for_routes,
                           const routeidx_t size,
                           routeidx_t *offset);
