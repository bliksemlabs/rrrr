/* polyline.c */
/* https://developers.google.com/maps/documentation/utilities/polylinealgorithm */

#include "polyline.h"
#include "geometry.h"
#include <stdint.h>

int encode_double (double c, char *buf) {
    char *b = buf;
    int32_t binary = (int32_t) (1e5 * c);
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
static latlon_t last_latlon;
static char  buf[BUFLEN];
static char *buf_cur;
static char *buf_max = buf + BUFLEN - 13; // each latlon could take up to 12 chars

void polyline_begin () {
    last_latlon.lat = 0.0;
    last_latlon.lon = 0.0;
    buf_cur = buf;
    *buf_cur = '\0';
}

void polyline_point (latlon_t point) {
    // avoid buffer overflow
    if (buf_cur >= buf_max) return; 
    latlon_t ll = point;
    // encoded polylines are variable-width. use coordinate differences to save space.
    ll.lat  -= last_latlon.lat;
    ll.lon  -= last_latlon.lon;
    buf_cur += encode_latlon (ll, buf_cur);
    last_latlon = point;
}

char *polyline_result () {
    return buf;
}

