#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "routes.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_routes_init (tdata_routes_t *routes, tdata_lines_t *lines)
{
    routes->line_for_route = NULL;

    routes->lines = lines;

    routes->size = 0;
    routes->len = 0;

    return ret_ok;
}

ret_t
tdata_routes_fake (tdata_routes_t *routes, const lineidx_t *line_for_route, const uint32_t len)
{
    if (len > ((routeidx_t) -1))
        return ret_error;

    routes->line_for_route = (lineidx_t *) line_for_route;

    routes->size = 0;
    routes->len = (routeidx_t) len;

    return ret_ok;
}

ret_t
tdata_routes_mrproper (tdata_routes_t *routes)
{
    if (unlikely (routes->size == 0 && routes->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (routes->line_for_route)
        free (routes->line_for_route);

    return tdata_routes_init (routes, routes->lines);
}

ret_t
tdata_routes_ensure_size (tdata_routes_t *routes, routeidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= routes->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (routes->line_for_route == NULL) {
        routes->line_for_route = (lineidx_t *) calloc (size, sizeof(lineidx_t));

        if (unlikely (routes->line_for_route == NULL)) {
            return ret_nomem;
        }
        routes->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (routes->line_for_route, sizeof(lineidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    routes->line_for_route = (lineidx_t *) p;

    routes->size = size;

    return ret_ok;
}

ret_t
tdata_routes_add (tdata_routes_t *routes,
                     const lineidx_t *line_for_route,
                     const routeidx_t size,
                     routeidx_t *offset)
{
    int available;

    if (unlikely (routes->size == 0 && routes->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = routes->size - routes->len;

    if ((routeidx_t) available < size) {
        if (unlikely (tdata_routes_ensure_size (routes, routes->size + (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (routes->line_for_route + routes->len, line_for_route, sizeof(lineidx_t) * size);

    if (offset) {
        *offset = routes->len;
    }

    routes->len += size;

    return ret_ok;
}
