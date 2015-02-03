/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* hashgrid.c */
#include "hashgrid.h"

#include "geometry.h"
#include "tdata.h"
#include "config.h"
#include <syslog.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h> /* be sure to link with math library (-lm) */

/* TODO: benchmark the conversion from variable length arrays
 * to [y * grid_dims + x]
 * evaluate if precomputing y might be more efficient.
 */

/* http://stackoverflow.com/a/859694/778449
 * cdecl> declare items as pointer to array 10 of pointer to void
 * void *(*items)[10]
 */

/* treat as unsigned to get 0-wrapping right? use only powers of 2 and
 * bitshifting/masking to bin?
 * ints are a circle coming back around to 0 after -1,
 * masking only the last B bits should wrap perfectly at
 * both 0 and +180 degrees (except that the internal coords are projected).
 * adding brad coords with overflow should also work,
 * but you have to make sure overflow is happening (-fwrapv?)
 */

static uint32_t xbin (hashgrid_t *hg, coord_t *coord) {
    uint32_t x = (uint32_t) abs(coord->x / (hg->bin_size.x));
    x %= hg->grid_dim;
    #ifdef RRRR_DEBUG_HASHGRID
    fprintf(stderr, "binning x coord %d, bin is %d \n", coord->x, x);
    #endif
    return x;
}

static uint32_t ybin (hashgrid_t *hg, coord_t *coord) {
    uint32_t y = (uint32_t) abs(coord->y / (hg->bin_size.y));
    y %= hg->grid_dim;
    #ifdef RRRR_DEBUG_HASHGRID
    fprintf(stderr, "binning y coord %d, bin is %d \n", coord->y, y);
    #endif
    return y;
}

void hashgrid_query (hashgrid_t *hg, hashgrid_result_t *result, coord_t coord, double radius_meters) {
    coord_t radius;
    result->coord = coord;
    result->radius_meters = radius_meters;
    coord_from_meters (&radius, radius_meters, radius_meters);
    result->hg = hg;

    /* TODO: misbehaves at 0? need wrap macro? */
    result->min.x = (coord.x - radius.x);
    result->min.y = (coord.y - radius.y);

    /* coord_subtract / coord_add */
    result->max.x = (coord.x + radius.x);
    result->max.y = (coord.y + radius.y);
    result->xmin = xbin (hg, &(result->min));
    result->ymin = ybin (hg, &(result->min));
    result->xmax = xbin (hg, &(result->max));
    result->ymax = ybin (hg, &(result->max));

    hashgrid_result_reset(result);
}

void hashgrid_result_reset (hashgrid_result_t *result) {
    result->x = result->xmin;
    result->y = result->ymin;

    /* TODO: should start at -1, which will increment to 0? */
    result->i = 0;
    result->has_next = true;
}

uint32_t hashgrid_result_next (hashgrid_result_t *r) {
    uint32_t ret_item;

    if ( ! (r->has_next))
        return HASHGRID_NONE;
    r->i += 1;
    if (r->i >= r->hg->counts[r->y * r->hg->grid_dim + r->x]) {
        r->i = 0;
        /* note '==': inequalities do not work due to wrapping */
        if (r->x == r->xmax) {
            r->x = r->xmin;
            if (r->y == r->ymax) {
                r->has_next = false;
                return HASHGRID_NONE;
            } else {
                r->y = (r->y + 1) % r->hg->grid_dim;
            }
        } else {
            r->x = (r->x + 1) % r->hg->grid_dim;
        }
    }

    ret_item = r->hg->bins[r->y * r->hg->grid_dim + r->x][r->i];
    #ifdef RRRR_DEBUG
    printf ("x=%d y=%d i=%d item=%d ", r->x, r->y, r->i, ret_item);
    #endif
    return ret_item;
}

/* Pre-filter the results, removing most false positives using a bounding box.
 * This will only work if the initial coordinate list is still available (was
 * not freed or did not go out of scope). We could also return a boolean to
 * indicate whether there is a result, and have an out-parameter for the index.
 *
 * The hashgrid can provide many false positives, but no false negatives
 * (what is the term?). A bounding box or the squared distance can both be
 * used to filter points. Note that most false positives are quite far away
 * so a bounding box is effective.
 */
uint32_t hashgrid_result_next_filtered (hashgrid_result_t *r, double *distance) {
    uint32_t item;
    while ((item = hashgrid_result_next(r)) != HASHGRID_NONE) {
        coord_t *coord = r->hg->coords + item;
        latlon_t latlon;
        latlon_from_coord (&latlon, coord);
        #ifdef RRRR_DEBUG_HASHGRID
        fprintf (stderr, "%f,%f\n", latlon.lat, latlon.lon);
        #endif
        if (coord->x > r->min.x && coord->x < r->max.x &&
            coord->y > r->min.y && coord->y < r->max.y) {
            /* item's coordinate is within bounding box,
             * calculate actual distance
             */
            *distance = coord_distance_meters (&(r->coord), coord);
            #ifdef RRRR_DEBUG_HASHGRID
            fprintf (stderr, "%d,%d,%f\n", coord->x, coord->y, *distance);
            #endif
            if (*distance < r->radius_meters) {
                return item;
            }
        }
    }
    return HASHGRID_NONE;
}

uint32_t hashgrid_result_closest (hashgrid_result_t *r) {
    uint32_t item;
    uint32_t best_item = HASHGRID_NONE;
    double   best_distance = INFINITY;
    while ((item = hashgrid_result_next(r)) != HASHGRID_NONE) {
        coord_t *coord = r->hg->coords + item;
        latlon_t latlon;
        latlon_from_coord (&latlon, coord);
        #ifdef RRRR_DEBUG_HASHGRID
        fprintf (stderr, "%f,%f\n", latlon.lat, latlon.lon);
        #endif
        if (coord->x > r->min.x && coord->x < r->max.x &&
            coord->y > r->min.y && coord->y < r->max.y) {
            /* false positives filtered,
             * item's coordinate is within bounding box
             */
            double distance = coord_distance_meters (&(r->coord), coord);
            if (distance < best_distance) {
                best_item = item;
                best_distance = distance;
            }
        }
    }
    return best_item;
}

void hashgrid_init (hashgrid_t *hg, uint32_t grid_dim, double bin_size_meters,
                    coord_t *coords, uint32_t n_items) {
    /* Initialize all struct members. */
    hg->grid_dim = grid_dim;
    hg->coords = coords;
    hg->bin_size_meters = bin_size_meters;
    coord_from_meters (&(hg->bin_size), bin_size_meters, bin_size_meters);
    hg->n_items = n_items;
    hg->counts = (uint32_t *) malloc(sizeof(uint32_t)  * grid_dim * grid_dim);
    hg->bins   = (uint32_t **) malloc(sizeof(uint32_t *) * grid_dim * grid_dim);
    hg->items  = (uint32_t *) malloc(sizeof(uint32_t) * n_items);

    {
        /* Initalize all dynamically allocated arrays. */
        uint32_t y;
        for (y = 0; y < grid_dim; ++y) {
            uint32_t x;
            for (x = 0; x < grid_dim; ++x) {
                hg->counts[y * grid_dim + x] = 0;
                hg->bins[y * grid_dim + x] = NULL;
            }
        }
    }

    {
        /* Count the number of items that will fall into each bin. */
        uint32_t i_coord;
        for (i_coord = 0; i_coord < n_items; ++i_coord) {
            #ifdef RRRR_DEBUG_HASHGRID
            fprintf(stderr, "binning coordinate x=%d y=%d \n",
                            (coords + i_coord)->x, (coords + i_coord)->y);
            #endif
            hg->counts[ybin(hg, coords + i_coord) * grid_dim +
                                xbin(hg, coords + i_coord)] += 1;
        }
    }

    {
        /* Set bin pointers to alias subregions of
         * the items array (which will be filled later).
         */
        uint32_t *bin = hg->items;
        uint32_t y;
        for (y = 0; y < grid_dim; ++y) {
            uint32_t x;
            for (x = 0; x < grid_dim; ++x) {
                hg->bins[y * grid_dim + x] = bin;
                bin += hg->counts[y * grid_dim + x];
                /* Reset bin item count for reuse when filling up bins. */
                hg->counts[y * grid_dim + x] = 0;
            }
        }
    }

    {
        /* Add the item indexes to the bins, which are pointers
         * to regions within the items array.
         */
        uint32_t i_coord;
        for (i_coord = 0; i_coord < n_items; ++i_coord) {
            coord_t *coord = coords + i_coord;
            uint32_t x = xbin (hg, coord);
            uint32_t y = ybin (hg, coord);
            uint32_t i = hg->counts[y * grid_dim + x];
            hg->bins[y * grid_dim + x][i] = i_coord;
            hg->counts[y * grid_dim + x] += 1;
        }
    }
}


void hashgrid_teardown (hashgrid_t *hg) {
    /* Free up dynamically allocated arrays.
     * Individual bins do not need to be freed separately from items.
     */
    free (hg->coords);
    free (hg->counts);
    free (hg->bins);
    free (hg->items);
}

#ifdef RRRR_DEBUG
void hashgrid_dump (hashgrid_t *hg) {
    uint32_t total, y;

    fprintf (stderr, "Hash Grid %dx%d \n", hg->grid_dim, hg->grid_dim);
    fprintf (stderr, "bin size: %f meters \n", hg->bin_size_meters);
    fprintf (stderr, "number of items: %d \n", hg->n_items);

    /* Grid of counts */
    total = 0;
    for (y = 0; y < hg->grid_dim; ++y) {
        uint32_t x;
        for (x = 0; x < hg->grid_dim; ++x) {
            fprintf (stderr, "%2d ", hg->counts[y * hg->grid_dim + x]);
            total += hg->counts[y * hg->grid_dim + x];
        }
        fprintf (stderr, "\n");
    }
    fprintf (stderr, "total of all counts: %d", total);
    /* Bins */
    for (y = 0; y < hg->grid_dim; ++y) {
        uint32_t x;
        for (x = 0; x < hg->grid_dim; ++x) {
            uint32_t i;
            fprintf (stderr, "Bin [%02d][%02d] ", y, x);
            for (i = 0; i < hg->counts[y * hg->grid_dim + x]; ++i) {
                fprintf (stderr, "%d ", hg->bins[y * hg->grid_dim + x][i]);
            }
            fprintf (stderr, "\n");
        }
    }
    fprintf (stderr, "\n");
}
#endif
