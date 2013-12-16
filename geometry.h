/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* geometry.h */

#ifndef _GEOMETRY_H
#define _GEOMETRY_H

#include <stdint.h>

typedef struct latlon latlon_t;
struct latlon {
    float lat;
    float lon;
};

typedef struct coord coord_t;
struct coord {
    int32_t x;
    int32_t y;
};

void coord_from_latlon (coord_t*, latlon_t*);

void coord_from_lat_lon (coord_t*, double lat, double lon);

void coord_from_meters (coord_t*, double meters_x, double meters_y);

double coord_distance_meters (coord_t*, coord_t*);

void latlon_dump (latlon_t*);

void latlon_from_coord (latlon_t*, coord_t*);

void coord_dump (coord_t*);

#endif // _GEOMETRY_H
