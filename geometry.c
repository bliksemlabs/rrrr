/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "geometry.h"
#include "rrrr_types.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

/* Must be scaled according to latitude for use in the longitude direction.
 */
#define METERS_PER_DEGREE_LAT (EARTH_CIRCUMFERENCE / 360.0)

double radians (double degrees) {
    return degrees * M_PI / 180;
}

double degrees (double radians) {
    return radians * 180 / M_PI;
}

/* Sinusoidal / equirectangular projection.
 * TODO: Replace with fast estimation polynomial.
 */
static double xscale_at_y (uint32_t y_brads) {
    return cos(y_brads * M_PI * 2 / UINT32_MAX);
}

static double xscale_at_lat (double latitude) {
    return cos(radians(latitude));
}

static double coord_diff_meters (int32_t o1, int32_t o2) {
     return (o2 - o1) * METERS_PER_BRAD;
}

/* Several mappings from lat/lon to integers to are possible.
 * The three major decisions are:
 * 1. Signed or unsigned brads.
 * 2. Zero uint32_t in brads is at the prime meridian or its antipode.
 * 3. Whether or not to store scaled x values.
 *
 * Design decisions and reflections:
 * If we map zero brads to 0 longitude and xvalues are not scaled, unsigned
 * and signed brads have identical binary representations. The main advantage
 * of using unsigned brads is that is prevents hashgrids from "reflecting"
 * around the origin when normal division is performed.
 * The unsigned conversion could be performed within the hashgrid itself, and
 * binning could be performed with masks (shift out bits within cell, zero out
 * high bits, rest is cell number).
 * If the x values are stored pre-scaled we will not get proper wrapping at
 * the date line, but bins will be of a consistent size.
 * If the x values are stored unscaled then we will get seamless wrapping at
 * the date line, but bins will not be of a consistent size.
 * Not all architectures will use twos complement signed integers... which
 * don't?
 * In order to efficiently calculate distances, we need the x and y
 * coordinates to be in the same units / at the same scale. We can either
 * sacrifice vertical resolution and store both coordinates at the x scale,
 * or perform an additional multiply operation at every distance calculation
 * to restore the y coordinate to the same scale.
 * 2. Either we start x=0 at lon=-180 or lon=0.
 */
void coord_from_lat_lon (coord_t *coord, double lat, double lon) {
    coord->y = lat * UINT32_MAX / 360.0;
    coord->x = lon * UINT32_MAX / 360.0 * xscale_at_lat (lat);
}

void coord_from_meters (coord_t *coord, double meters_x, double meters_y) {
    coord->x = meters_x / METERS_PER_BRAD;
    coord->y = meters_y / METERS_PER_BRAD;
}

void coord_from_latlon (coord_t *coord, latlon_t *latlon) {
    coord_from_lat_lon (coord, latlon->lat, latlon->lon);
}

/* Returns a monotone increasing function of the distance between points that
 * is faster to calculate. This is an order-preserving map, so it can be used
 * to establish closer/farther relationships. It is implemented as the square
 * of the distance in latitude-scaled/projected binary radians.
 *
 * TODO: add meters_from_ersatz
 */
double coord_distance_ersatz (coord_t *c1, coord_t *c2) {
    double dx = c2->x - c1->x;
    double dy = c2->y - c1->y;
    return (dx * dx) + (dy * dy);
}

/* Returns the equivalent ersatz distance for a given number of meters,
 * for distance comparisons.
 */
double ersatz_from_distance (double meters) {
    double d_brads = meters / METERS_PER_BRAD ;
    return d_brads * d_brads;
}

double coord_distance_meters (coord_t *c1, coord_t *c2) {
    double dxm = coord_diff_meters(c1->x, c2->x);
    double dym = coord_diff_meters(c1->y, c2->y);
    return sqrt((dxm * dxm) + (dym * dym));
}

double latlon_distance_meters (latlon_t *ll1, latlon_t *ll2) {
    /* Rather than finding and using the average latitude for longitude
     * scaling, or scaling the two latlons separately, just convert the to
     * the internal projected representation.
     */
    coord_t c1, c2;
    coord_from_latlon (&c1, ll1);
    coord_from_latlon (&c2, ll2);
    return coord_distance_meters (&c1, &c2);
}

void latlon_from_coord (latlon_t *latlon, coord_t *coord) {
    latlon->lat = coord->y * 180.0f / INT32_MAX ;
    latlon->lon = coord->x * 180.0f / INT32_MAX / xscale_at_y (coord->y);
}

bool strtolatlon (char *latlon, latlon_t *result) {
    char *endptr;
    result->lat = (float) strtod(latlon, &endptr);
    if (endptr && endptr[1]) {
        result->lon = (float) strtod(&endptr[1], NULL);
        return true;
    }
    return false;
}

#ifdef RRRR_DEBUG
void latlon_dump (latlon_t *latlon) {
    fprintf(stderr, "latlon lat=%f lon=%f \n", latlon->lat, latlon->lon);
}

void coord_dump (coord_t *coord) {
    fprintf(stderr, "coordinate x=%d y=%d \n", coord->x, coord->y);
}
#endif
