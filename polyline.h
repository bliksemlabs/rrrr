/* polyline.h */
/* https://developers.google.com/maps/documentation/utilities/polylinealgorithm */

//#include "geometry.h"
typedef struct {
    double lat;
    double lon;
} latlon_t;

int encode_double (double c, char *buf);

int encode_latlon (latlon_t ll, char *buf);

void polyline_begin ();

void polyline_point (latlon_t point);

char *polyline_result ();


