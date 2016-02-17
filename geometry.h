/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#ifndef _GEOMETRY_H
#define _GEOMETRY_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* Mean of Earth's equatorial and meridional circumferences. */
#define EARTH_CIRCUMFERENCE 40041438.5

/* UINT32_MAX is also the full range of INT32. */
#define INT32_RANGE UINT32_MAX

/* We could have more resolution in the latitude direction by mapping
 * 90 degrees to the int32 range instead of 180, but keeping both axes at the
 * same scale enables efficent distance calculations. In any case the extra
 * Y resolution is unnecessary, since 1 brad is already just under 1cm.
 */
#define METERS_PER_BRAD (EARTH_CIRCUMFERENCE / INT32_RANGE)

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

double coord_distance_meters (const coord_t*, const coord_t*);

double latlon_distance_meters (latlon_t *ll1, latlon_t *ll2);

double coord_distance_ersatz (const coord_t *c1, const coord_t *c2);

double ersatz_from_distance (double meters);

void latlon_dump (latlon_t*);

void latlon_from_coord (latlon_t*, const coord_t*);

void coord_dump (const coord_t*);

bool strtolatlon (char *latlon, latlon_t *result);

#endif /* _GEOMETRY_H */
