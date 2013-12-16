/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* polyline.h */
/* https://developers.google.com/maps/documentation/utilities/polylinealgorithm */

#include "geometry.h"
#include "tdata.h"
#include "router.h"

int encode_double (double c, char *buf);

int encode_latlon (latlon_t ll, char *buf);

void polyline_begin ();

void polyline_point (double lat, double lon);

void polyline_latlon (latlon_t ll);

char *polyline_result (); // this could just be a global variable

uint32_t polyline_length (); // number of points in the polyline

void polyline_for_leg (tdata_t *tdata, struct leg *leg);
