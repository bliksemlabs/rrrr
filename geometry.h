/* geometry.h */

typedef struct coord coord_t;
struct coord {
    int32_t x;
    int32_t y;
};

void coord_from_degrees(coord_t coord, double lat, double lon);

