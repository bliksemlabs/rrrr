/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* hashgrid.h */

#ifndef _HASHGRID_H
#define _HASHGRID_H

#include "geometry.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#define HASHGRID_NONE UINT32_MAX

#ifndef INFINITY
#define INFINITY 9999999.0
#endif

typedef struct hashgrid_s hashgrid_t;
struct hashgrid_s {
    /* the array of coords that were indexed
     * note: may have been deallocated by caller
     */
    coord_t *coords;

    /* array containing all the binned items,
     * aliased by the bins array
     */
    uint32_t     *items;

    /* 2D array of counts */
    uint32_t     *counts;

    /* 2D array of uint32_t pointers */
    uint32_t     **bins;

    double       bin_size_meters;
    coord_t      bin_size;
    uint32_t     grid_dim;
    uint32_t     n_items;
};

typedef struct hashgrid_result_s hashgrid_result_t;
struct hashgrid_result_s {
    hashgrid_t *hg;

    /* query radius in meters */
    double radius_meters;

    /* the query coordinate */
    coord_t coord;

    /* defines a bounding box around the result in projected brads */
    coord_t min, max;

    /* bins that correspond to the bounding box */
    uint32_t xmin, xmax, ymin, ymax;

    /* current position within the hashgrid for iterating over results */
    uint32_t x, y, i;
    bool has_next;
};

void hashgrid_init (hashgrid_t *hg, uint32_t grid_dim, double bin_size_meters, coord_t *coords, uint32_t n_items);

void hashgrid_query (hashgrid_t *, hashgrid_result_t *, coord_t, double radius_meters);

void hashgrid_teardown (hashgrid_t *);

void hashgrid_result_reset (hashgrid_result_t *);

uint32_t hashgrid_result_next_filtered (hashgrid_result_t *r, double *distance);

uint32_t hashgrid_result_next (hashgrid_result_t *r);

uint32_t hashgrid_result_closest (hashgrid_result_t *r);

#ifdef RRRR_DEBUG
void hashgrid_dump (hashgrid_t *);
#endif

#endif /* _HASHGRID_H */
