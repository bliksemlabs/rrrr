/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* hashgrid.h */

#ifndef _HASHGRID_H
#define _HASHGRID_H

#include "geometry.h"
#include <stdint.h>
#include <stdbool.h>

#define HASHGRID_NONE UINT32_MAX

#ifndef INFINITY
#define INFINITY 9999999.0
#endif

typedef struct HashGrid {
    uint32_t     grid_dim;
    double       bin_size_meters;
    coord_t      bin_size;
    uint32_t     n_items;

    /* 2D array of counts */
    uint32_t     *counts;

    /* 2D array of uint32_t pointers */
    uint32_t     **bins;

    /* array containing all the binned items,
     * aliased by the bins array
     */
    uint32_t     *items;

    /* the array of coords that were indexed
     * note: may have been deallocated by caller
     */
    coord_t *coords;
} HashGrid;

typedef struct HashGridResult {
    HashGrid *hg;

    /* the query coordinate */
    coord_t coord;

    /* query radius in meters */
    double radius_meters;

    /* defines a bounding box around the result in projected brads */
    coord_t min, max;

    /* bins that correspond to the bounding box */
    uint32_t xmin, xmax, ymin, ymax;

    /* current position within the hashgrid for iterating over results */
    uint32_t x, y, i;
    bool has_next;
} HashGridResult;

void HashGrid_init (HashGrid *hg, uint32_t grid_dim, double bin_size_meters, coord_t *coords, uint32_t n_items);

void HashGrid_dump (HashGrid*);

void HashGrid_query (HashGrid*, HashGridResult*, coord_t, double radius_meters);

void HashGrid_teardown (HashGrid*);

void HashGridResult_reset (HashGridResult*);

uint32_t HashGridResult_next_filtered (HashGridResult *r, double *distance);

uint32_t HashGridResult_closest (HashGridResult *r);

#endif /* _HASHGRID_H */
