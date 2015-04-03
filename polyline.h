/* Copyright 2013-2015 Bliksem Labs B.V.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* polyline.h */
/* https://developers.google.com/maps/documentation/utilities/polylinealgorithm */

#include "geometry.h"
#include "router_result.h"

/* For The Netherlands our longest journey pattern is 124 stops
 * it would be sufficient to allocate only 124 * 13 bytes.
 */

#define PL_BUFLEN 1996

typedef struct polyline polyline_t;
struct polyline {
    char  *buf_cur;
    char  *buf_max;
    uint32_t last_lat;
    uint32_t last_lon;
    uint32_t n_points;
    char   buf[PL_BUFLEN];
};

void polyline_begin (polyline_t *pl);

void polyline_point (polyline_t *pl, double lat, double lon);

void polyline_latlon (polyline_t *pl, latlon_t ll);

char *polyline_result (polyline_t *pl);

/* number of points in the polyline */
uint32_t polyline_length (polyline_t *pl);
