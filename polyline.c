/* polyline.c */

/* 
  https://developers.google.com/maps/documentation/utilities/polylinealgorithm 
  https://developers.google.com/maps/documentation/utilities/polylineutility 
*/

#include "polyline.h"
#include "geometry.h"
#include "tdata.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


// TODO width of 'binary' and precision of dlon, dlat

int encode_double (double c, char *buf) {
    char *b = buf;
    int64_t binary = (1e5 * c);
    //printf ("%d \n", binary);
    binary <<= 1;
    if (c < 0) binary = ~binary;
    // 31 == (2^5 - 1) == 00000 00000 00000 00000 00000 11111
    uint32_t mask = 31;
    uint8_t chunks[6];
    for (int i = 0; i < 6; ++i) {
        chunks[i] = (binary & mask) >> (5 * i);
        mask <<= 5;
    }
    for (int i = 0; i < 6; ++i) {
        if (i != 5 && chunks[i+1] != 0) chunks[i] |= 0x20;
        chunks[i] += 63;
        *(b++) = (char) chunks[i];            
        if (chunks[i+1] == 0) break;
    }
    *b = '\0';
    return (b - buf);
}

/* Our latlon_t contains floats, so results will not be exactly like examples. */
int encode_latlon (latlon_t ll, char *buf) {
    int nc = 0;
    nc += encode_double (ll.lat, buf);
    nc += encode_double (ll.lon, buf + nc);
    return nc;
}

/* Internal buffer for building up polylines point by point. */

#define BUFLEN 1024
static latlon_t last_point;
static char  buf[BUFLEN];
static char *buf_cur;
static char *buf_max = buf + BUFLEN - 13; // each latlon could take up to 12 chars

void polyline_begin () {
    last_point.lat = 0.0;
    last_point.lon = 0.0;
    buf_cur = buf;
    *buf_cur = '\0';
}

void polyline_point (latlon_t point) {
    // avoid buffer overflow
    if (buf_cur >= buf_max) return; 
    // encoded polylines are variable-width. use coordinate differences to save space.
    double dlat = (double) (point.lat) - (double) (last_point.lat);
    double dlon = (double) (point.lon) - (double) (last_point.lon);
//    buf_cur += encode_latlon (ll, buf_cur);
    buf_cur += encode_double (dlat, buf_cur);
    buf_cur += encode_double (dlon, buf_cur);
    last_point = point;
}

char *polyline_result () {
    return buf;
}

/*
  Produces a polyline connecting a subset of the stops in a route.
  s0 and s1 are global stop indexes, not stop indexes within the route.
*/
void polyline_for_ride (tdata_t *tdata, uint32_t route_idx, uint32_t sidx0, uint32_t sidx1) {
    route_t route = tdata->routes[route_idx];
    uint32_t *stops = tdata_stops_for_route (*tdata, route_idx);
    bool output = false;
    polyline_begin();
    for (int s = 0; s < route.n_stops; ++s) {
        uint32_t sidx = stops[s];
        if (!output && (sidx == sidx0)) output = true;
        if (output) polyline_point (tdata->stop_coords[sidx]);
        if (sidx == sidx1) break;
    }
    printf ("%s\n", polyline_result ());
}

