/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _GEOMETRY_H
#define _GEOMETRY_H

#include "rrrr_types.h"

void coord_from_latlon (coord_t*, latlon_t*);

void coord_from_lat_lon (coord_t*, double lat, double lon);

void coord_from_meters (coord_t*, double meters_x, double meters_y);

double coord_distance_meters (coord_t*, coord_t*);

double coord_distance_ersatz (coord_t *c1, coord_t *c2);

double ersatz_from_distance (double meters);

void latlon_dump (latlon_t*);

void latlon_from_coord (latlon_t*, coord_t*);

void coord_dump (coord_t*);

#endif /* _GEOMETRY_H */
