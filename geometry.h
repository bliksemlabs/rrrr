/* geometry.h */

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

void coord_from_degrees(coord_t*, double lat, double lon);

void coord_from_meters (coord_t*, double meters_x, double meters_y);

