/* hashgrid.h */

#include "geometry.h"
#include <malloc.h>

void HashGrid_init (HashGrid*, int bin_size_meters, int grid_dim);

void HashGrid_count (HashGrid*, coord*);

void HashGrid_teardown(HashGrid*);
