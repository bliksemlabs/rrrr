/* polyline.h */
/* https://developers.google.com/maps/documentation/utilities/polylinealgorithm */

#include "geometry.h"
#include "tdata.h"

int encode_double (double c, char *buf);

int encode_latlon (latlon_t ll, char *buf);

void polyline_begin ();

void polyline_point (latlon_t point);

char *polyline_result (); // this could just be a global variable

uint32_t polyline_length (); // this could just be a global variable

void polyline_for_ride (tdata_t *tdata, uint32_t route_idx, uint32_t sidx0, uint32_t sidx1);

