/* hashgrid.h */

#ifndef _HASHGRID_H
#define _HASHGRID_H

#include "geometry.h"
#include <stdint.h>

#define HASHGRID_NONE UINT32_MAX

typedef struct HashGrid HashGrid;

typedef struct HashGridResult HashGridResult;

void HashGrid_init (HashGrid *hg, uint32_t grid_dim, double bin_size_meters, coord_t *coords, uint32_t n_items);

void HashGrid_dump (HashGrid*);

void HashGrid_query (HashGrid*, HashGridResult*, coord_t, double radius_meters);

void HashGrid_teardown (HashGrid*);

#endif // _HASHGRID_H
