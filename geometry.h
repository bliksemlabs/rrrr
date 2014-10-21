/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _GEOMETRY_H
#define _GEOMETRY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct coord coord_t;
struct coord {
    int32_t x;
    int32_t y;
};

typedef struct latlon latlon_t;
struct latlon {
    float lat;
    float lon;
};

void coord_from_latlon (coord_t*, latlon_t*);

void coord_from_lat_lon (coord_t*, double lat, double lon);

void coord_from_meters (coord_t*, double meters_x, double meters_y);

double coord_distance_meters (coord_t*, coord_t*);

double coord_distance_ersatz (coord_t *c1, coord_t *c2);

double ersatz_from_distance (double meters);

void latlon_dump (latlon_t*);

void latlon_from_coord (latlon_t*, coord_t*);

void coord_dump (coord_t*);

bool strtolatlon (char *latlon, latlon_t *result);

#endif /* _GEOMETRY_H */
