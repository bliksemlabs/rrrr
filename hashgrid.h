/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* hashgrid.h */

#ifndef _HASHGRID_H
#define _HASHGRID_H

#include "geometry.h"
#include <stdint.h>
#include <stdbool.h>

#define HASHGRID_NONE UINT32_MAX

typedef struct HashGrid {
    uint32_t     grid_dim;
    double  bin_size_meters;
    coord_t bin_size;
    uint32_t     n_items;
    uint32_t     (*counts)[]; // 2D array of counts
    uint32_t     *(*bins)[];  // 2D array of uint32_t pointers
    uint32_t     *items;      // array containing all the binned items, aliased by the bins array
    coord_t *coords;          // the array of coords that were indexed (note: may have been deallocated by caller)
} HashGrid;

typedef struct HashGridResult {
    HashGrid *hg;
    coord_t coord;                   // the query coordinate
    double radius_meters;            // query radius in meters
    coord_t min, max;                // defines a bounding box around the result in projected brads
    uint32_t xmin, xmax, ymin, ymax; // bins that correspond to the bounding box
    uint32_t x, y, i;                // current position within the hashgrid for iterating over results
    bool has_next;
} HashGridResult;

void HashGrid_init (HashGrid *hg, uint32_t grid_dim, double bin_size_meters, coord_t *coords, uint32_t n_items);

void HashGrid_dump (HashGrid*);

void HashGrid_query (HashGrid*, HashGridResult*, coord_t, double radius_meters);

void HashGrid_teardown (HashGrid*);

uint32_t HashGridResult_next_filtered (HashGridResult *r, double *distance);

uint32_t HashGridResult_closest (HashGridResult *r);

#endif // _HASHGRID_H
