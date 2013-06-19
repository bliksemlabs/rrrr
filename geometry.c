#include "geometry.h"
#include <math.h>

/* geometry functions */

#define EARTH_EQUATORIAL_CIRCUMFERENCE 40075017.0
#define EARTH_MERIDIONAL_CIRCUMFERENCE 40007860.0
#define INT32_RANGE (2.0 * INT32_MAX - 1.0)
#define METERS_PER_BRAD_X (EARTH_EQUATORIAL_CIRCUMFERENCE / INT32_RANGE)
#define METERS_PER_BRAD_Y (EARTH_MERIDIONAL_CIRCUMFERENCE / 2.0 / INT32_RANGE)

double radians (double degrees) {
    return degrees * M_PI / 180.0;
}

double degrees (double radians) {
    return radians * 180.0 / M_PI;
}

void coord_from_degrees (coord_t *coord, double lat, double lon) {
    coord->y = lat * INT32_MAX / 90.0;
    coord->x = lon * INT32_MAX / 180.0 * cos(radians(lat));
}

void coord_from_meters (coord_t *coord, double meters_x, double meters_y) {
    coord->y = meters_y / METERS_PER_BRAD_Y; 
    coord->x = meters_x / METERS_PER_BRAD_X; 
}

double coord_xdiff_meters (coord_t *c1, coord_t *c2) {
    return (c2->x - c1->x) * METERS_PER_BRAD_X;
}

double coord_ydiff_meters (coord_t *c1, coord_t *c2) {
    return (c2->y - c1->y) * METERS_PER_BRAD_Y;
}

// Returns a monotone increasing function of the distance between points that is faster to calculate.
// This is an order-preserving map, so it can be used to establish closer/farther relationships.
double coord_ersatz_distance (coord_t *c1, coord_t *c2) {
    double dx = c2->x - c1->x;
    double dy = c2->y - c1->y;
    return dx * dx + dy * dy;
}

double coord_distance_meters (coord_t *c1, coord_t *c2) {
    double dxm = coord_xdiff_meters(c1, c2);
    double dym = coord_ydiff_meters(c1, c2);
    return sqrt(dxm * dxm + dym * dym);
}

