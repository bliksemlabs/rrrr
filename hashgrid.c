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
#include <math.h> // be sure to link with math library (-lm) 

// http://stackoverflow.com/a/859694/778449
// cdecl> declare items as pointer to array 10 of pointer to void
// void *(*items)[10]

// treat as unsigned to get 0-wrapping right? use only powers of 2 and bitshifting/masking to bin?
// ints are a circle coming back around to 0 after -1, 
// masking only the last B bits should wrap perfectly at both 0 and +180 degrees (except that the internal coords are projected).
// adding brad coords with overflow should also work, but you have to make sure overflow is happening (-fwrapv?)
static inline uint32_t xbin (HashGrid *hg, coord_t *coord) {
    uint32_t x = abs(coord->x / (hg->bin_size.x)); 
    x %= hg->grid_dim;
    T printf("binning x coord %d, bin is %d \n", coord->x, x);
    return x;
}

static inline uint32_t ybin (HashGrid *hg, coord_t *coord) {
    uint32_t y = abs(coord->y / (hg->bin_size.y));
    y %= hg->grid_dim;
    T printf("binning y coord %d, bin is %d \n", coord->y, y);
    return y;
}

void HashGrid_query (HashGrid *hg, HashGridResult *result, coord_t coord, double radius_meters) {
    result->coord = coord;
    result->radius_meters = radius_meters;
    coord_t radius;
    coord_from_meters (&radius, radius_meters, radius_meters);
    result->hg = hg;
    result->min.x = (coord.x - radius.x); // misbehaves at 0? need wrap macro?
    result->min.y = (coord.y - radius.y);
    result->max.x = (coord.x + radius.x); // coord_subtract / coord_add
    result->max.y = (coord.y + radius.y);
    result->xmin = xbin (hg, &(result->min));
    result->ymin = ybin (hg, &(result->min));
    result->xmax = xbin (hg, &(result->max));
    result->ymax = ybin (hg, &(result->max));
    result->x = result->xmin;
    result->y = result->ymin;
    result->i = 0; // should start at -1, which will increment to 0?
    result->has_next = true;
}

uint32_t HashGridResult_next (HashGridResult *r) {
    if ( ! (r->has_next))
        return HASHGRID_NONE;
    uint32_t (*counts)[r->hg->grid_dim] = r->hg->counts;
    uint32_t *(*bins)[r->hg->grid_dim] = r->hg->bins;
    r->i += 1;
    if (r->i >= counts[r->y][r->x]) {
        r->i = 0;
        if (r->x == r->xmax) { // note '==': inequalities do not work due to wrapping
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
    uint32_t item = bins[r->y][r->x][r->i];
    // printf ("x=%d y=%d i=%d item=%d ", r->x, r->y, r->i, item);
    return item;
}

/*
  Pre-filter the results, removing most false positives using a bounding box. This will only work if 
  the initial coordinate list is still available (was not freed or did not go out of scope). We 
  could also return a boolean to indicate whether there is a result, and have an out-parameter 
  for the index.
  The hashgrid can provide many false positives, but no false negatives (what is the term?).
  A bounding box or the squared distance can both be used to filter points. 
  Note that most false positives are quite far away so a bounding box is effective.
*/
uint32_t HashGridResult_next_filtered (HashGridResult *r, double *distance) {
    uint32_t item;
    while ((item = HashGridResult_next(r)) != HASHGRID_NONE) {
        coord_t *coord = r->hg->coords + item;
        latlon_t latlon;
        latlon_from_coord (&latlon, coord);
        // printf ("%f,%f\n", latlon.lat, latlon.lon);
        if (coord->x > r->min.x && coord->x < r->max.x && 
            coord->y > r->min.y && coord->y < r->max.y) {
            // item's coordinate is within bounding box, calculate actual distance
            *distance = coord_distance_meters (&(r->coord), coord);
            //printf ("%d,%d,%f\n", coord->x, coord->y, *distance);
            if (*distance < r->radius_meters) {
                return item;
            }
        }
    }
    return HASHGRID_NONE;
}

uint32_t HashGridResult_closest (HashGridResult *r) {
    uint32_t item;
    uint32_t best_item = HASHGRID_NONE;
    double   best_distance = INFINITY;
    while ((item = HashGridResult_next(r)) != HASHGRID_NONE) {
        coord_t *coord = r->hg->coords + item;
        latlon_t latlon;
        latlon_from_coord (&latlon, coord);
        // printf ("%f,%f\n", latlon.lat, latlon.lon);
        if (coord->x > r->min.x && coord->x < r->max.x && 
            coord->y > r->min.y && coord->y < r->max.y) {
            // false positives filtered, item's coordinate is within bounding box
            double distance = coord_distance_meters (&(r->coord), coord);
            if (distance < best_distance) {
                best_item = item;
                best_distance = distance;
            }
        }
    }
    return best_item;
}

void HashGrid_init (HashGrid *hg, uint32_t grid_dim, double bin_size_meters, coord_t *coords, uint32_t n_items) {
    // Initialize all struct members.
    hg->grid_dim = grid_dim;
    hg->coords = coords;
    hg->bin_size_meters = bin_size_meters;
    coord_from_meters (&(hg->bin_size), bin_size_meters, bin_size_meters);
    hg->n_items = n_items;
    hg->counts = malloc(sizeof(uint32_t)  * grid_dim * grid_dim);
    hg->bins   = malloc(sizeof(uint32_t *) * grid_dim * grid_dim);
    hg->items  = malloc(sizeof(uint32_t) * n_items);
    // Initalize all dynamically allocated arrays.
    uint32_t (*counts)[grid_dim] = hg->counts;
    uint32_t  *(*bins)[grid_dim] = hg->bins;
    for (uint32_t y = 0; y < grid_dim; ++y) {
        for (uint32_t x = 0; x < grid_dim; ++x) {
            counts[y][x] = 0;
            bins  [y][x] = NULL;
        }
    }
    // Count the number of items that will fall into each bin.
    for (uint32_t c = 0; c < n_items; ++c) {
        T printf("binning coordinate x=%d y=%d \n", (coords + c)->x, (coords + c)->y);
        counts[ybin(hg, coords + c)][xbin(hg, coords + c)] += 1;
    }
    // Set bin pointers to alias subregions of the items array (which will be filled later).
    uint32_t *bin = hg->items; 
    for (uint32_t y = 0; y < grid_dim; ++y) {
        for (uint32_t x = 0; x < grid_dim; ++x) {
            bins[y][x] = bin;
            bin += counts[y][x];
            // Reset bin item count for reuse when filling up bins.
            counts[y][x] = 0; 
        }
    }
    // Add the item indexes to the bins, which are pointers to regions within the items array.
    for (uint32_t c = 0; c < n_items; ++c) {
        coord_t *coord = coords + c;
        uint32_t x = xbin (hg, coord);
        uint32_t y = ybin (hg, coord);
        uint32_t i = counts[y][x];
        bins[y][x][i] = c;
        counts[y][x] += 1;
    }
}

void HashGrid_dump (HashGrid* hg) {
    printf ("Hash Grid %dx%d \n", hg->grid_dim, hg->grid_dim);
    printf ("bin size: %f meters \n", hg->bin_size_meters);
    printf ("number of items: %d \n", hg->n_items);
    uint32_t (*counts)[hg->grid_dim] = hg->counts;
    uint32_t  *(*bins)[hg->grid_dim] = hg->bins;
    // uint32_t *items = hg->items;
    // Grid of counts
    uint32_t total = 0;
    for (uint32_t y = 0; y < hg->grid_dim; ++y) {
        for (uint32_t x = 0; x < hg->grid_dim; ++x) {
            printf ("%2d ", counts[y][x]);
            total += counts[y][x];
        }
        printf ("\n");
    }
    printf ("total of all counts: %d", total);
    // Bins
    for (uint32_t y = 0; y < hg->grid_dim; ++y) {
        for (uint32_t x = 0; x < hg->grid_dim; ++x) {
            printf ("Bin [%02d][%02d] ", y, x);
            for (uint32_t i = 0; i < counts[y][x]; ++i) {
                printf ("%d ", bins[y][x][i]);
            }
            printf ("\n");
        }
    }
    printf ("\n");
}

void HashGrid_teardown(HashGrid *hg) {
    // Free up dynamically allocated arrays. 
    // Individual bins do not need to be freed separately from items.
    free (hg->counts);
    free (hg->bins);
    free (hg->items);
}

