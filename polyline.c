/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* polyline.c : encode chains of latitude/longitude coordinates as printable ASCII text. */

/*
  https://developers.google.com/maps/documentation/utilities/polylinealgorithm
  This official polyline description is a very imperative, algorithmic one and is missing some details.
  Here's my version:
  Encoded polylines are variable-length base-64 encoding of latitude and longitude coordinates. Each coordinate is
  encoded separately using exactly the same method, in pairs (lat, lon). The first pair in a polyline is absolute,
  but subsequent ones are relative. Saving deltas between points makes the overall polyline much shorter since we use
  only as many bytes as necessary for each point. The latitude and longitude are first converted to integers: from
  floating point to fixed-point (5 decimal places). The twos-complement representation of the number is left-shifted by
  one and inverted (bitwise NOT) if it is negative. This has the effect of making the lowest-order bit a sign bit and
  makes the number of significant bits proportionate to the absolute value of the number. Starting from the low-order
  bits we mask off chunks of 5 bits, giving a range of 0-31 for each chunk. Chunks of zeros in the high order bits are
  dropped, with the sixth bit (0x20) used to indicate whether additional chunks follow. These six-bit chunks have a
  range of 0-63, and are shifted by 63 into the printable character block at 63-126, with characters from the second
  half at 95-126 making up the body of an encoded number and characters from the first half at 63-94 (for which bit
  6 is 0) terminating an encoded number.

  Results can be checked with: https://developers.google.com/maps/documentation/utilities/polylineutility

  For comparison see: https://developers.google.com/protocol-buffers/docs/encoding#optional
  Repeated base-128 varints would be more efficient.
*/

#include "polyline.h"
#include "geometry.h"
#include "tdata.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

int encode_double (double c, char *buf) {
    char *b = buf;
    uint32_t binary = 1e5 * c;
    // printf ("%+3.5f %+8d ", c, binary);
    // use the lowest order bit as a sign bit
    binary <<= 1;
    // printf ("%+8d ", binary);
    if (c < 0) binary = ~binary;
    // printf ("%+8d ", binary);
    // 31 == (2^5 - 1) == 00000 00000 00000 00000 00000 11111
    uint32_t mask = 31;
    char chunks[6];
    // initialization to 0 is important so we always get at least one '?' chunk, allowing zero coordinates and deltas
    int last_chunk = 0;
    for (int i = 0; i < 6; ++i) {
        chunks[i] = (binary & mask) >> (5 * i);
        // printf ("%3d ", chunks[i]);
        // track the last nonzero chunk. there may be zeros between positive chunks (rendered as '_' == 95)
        if (chunks[i] != 0) last_chunk = i;
        mask <<= 5;
    }
    for (int i = 0; i <= last_chunk; ++i) {
        char out = chunks[i];
        if (i != last_chunk) out |= 0x20; // high bit (range 32-63) indicates another chunk follows
        out += 63; // move into alphabetic range 63-126, with 95-126 indicating another chunk follows
        *(b++) = out;
    }
    *b = '\0';
    // printf ("%s \n", buf);
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
static double last_lat;
static double last_lon;
static char  buf[BUFLEN];
static char *buf_cur;
static char *buf_max = buf + BUFLEN - 13; // each latlon could take up to 12 chars
static uint32_t n_points;

void polyline_begin () {
    last_lat = 0.0;
    last_lon = 0.0;
    buf_cur = buf;
    *buf_cur = '\0';
    n_points = 0;
}

/* Allows preserving precision by not using float-based latlon_t. */
void polyline_point (double lat, double lon) {
    // check for potential buffer overflow
    if (buf_cur >= buf_max) return;
    // encoded polylines are variable-width, and use coordinate differences to save space.
    double dlat = lat - last_lat;
    double dlon = lon - last_lon;
    buf_cur += encode_double (dlat, buf_cur);
    buf_cur += encode_double (dlon, buf_cur);
    last_lat = lat;
    last_lon = lon;
    n_points += 1;
}

void polyline_latlon (latlon_t ll) {
    polyline_point (ll.lat, ll.lon);
}

char *polyline_result () {
    return buf;
}

uint32_t polyline_length () {
    return n_points;
}

/*
  Produces a polyline connecting a subset of the stops in a route,
  or connecting two walk path endpoints if route_idx == WALK.
  sidx0 and sidx1 are global stop indexes, not stop indexes within the route.
*/
void polyline_for_leg (tdata_t *tdata, struct leg *leg) {
    polyline_begin();
    if (leg->route == WALK) {
        polyline_latlon (tdata->stop_coords[leg->s0]);
        polyline_latlon (tdata->stop_coords[leg->s1]);
    } else {
        rrrr_route_t route = tdata->routes[leg->route];
        uint32_t *stops = tdata_stops_for_route (tdata, leg->route);
        bool output = false;
        for (int s = 0; s < route.n_stops; ++s) {
            uint32_t sidx = stops[s];
            if (!output && (sidx == leg->s0)) output = true;
            if (output) polyline_latlon (tdata->stop_coords[sidx]);
            if (sidx == leg->s1) break;
        }
    }
    // printf ("final polyline: %s\n\n", polyline_result ());
}

