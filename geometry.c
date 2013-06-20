/* geometry.c */
#include "geometry.h"

#include "config.h"
#include <math.h>
#include <stdio.h>

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

void coord_from_latlon (coord_t *coord, latlon_t *latlon) {
    T latlon_dump (latlon);
    coord_from_degrees (coord, latlon->lat, latlon->lon);
    T coord_dump (coord);
}

void coord_from_degrees (coord_t *coord, double lat, double lon) {
    T printf ("coord from degrees: lat %f lon %f \n", lat, lon);
    coord->y = lat * INT32_MAX / 90.0;
    coord->x = lon * INT32_MAX / 180.0 * cos(radians(lat));
    T printf ("coord from degrees: x=%d y=%d \n", coord->x, coord->y);
}

void coord_from_meters (coord_t *coord, double meters_x, double meters_y) {
    coord->y = meters_y / METERS_PER_BRAD_Y; 
    coord->x = meters_x / METERS_PER_BRAD_X; 
    T printf ("coord from meters: x=%d y=%d \n", coord->x, coord->y);
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

void latlon_from_coord (latlon_t *latlon, coord_t *coord) {
    latlon->lat = coord->y *  90.0 / INT32_MAX ;
    latlon->lon = coord->x * 180.0 / INT32_MAX / cos(radians(latlon->lat));
}

void latlon_dump (latlon_t *latlon) {
    printf("latlon lat=%f lon=%f \n", latlon->lat, latlon->lon);
}

void coord_dump (coord_t *coord) {
    printf("coordinate x=%d y=%d \n", coord->x, coord->y);
}

