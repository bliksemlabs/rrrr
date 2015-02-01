/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* polyline.c : encode chains of latitude/longitude coordinates as
 * printable ASCII text.
 */

/* https://developers.google.com/maps/documentation/utilities/polylinealgorithm
 * This official polyline description is a very imperative, algorithmic one and
 * is missing some details.
 *
 * Here's my version:
 * Encoded polylines are variable-length base-64 encoding of latitude and
 * longitude coordinates. Each coordinate is encoded separately using exactly
 * the same method, in pairs (lat, lon). The first pair in a polyline is
 * absolute, but subsequent ones are relative. Saving deltas between points
 * makes the overall polyline much shorter since we use only as many bytes as
 * necessary for each point. The latitude and longitude are first converted to
 * integers: from floating point to fixed-point (5 decimal places). The
 * twos-complement representation of the number is left-shifted by one and
 * inverted (bitwise NOT) if it is negative. This has the effect of making the
 * lowest-order bit a sign bit and makes the number of significant bits
 * proportionate to the absolute value of the number. Starting from the
 * low-order bits we mask off chunks of 5 bits, giving a range of 0-31 for
 * each chunk. Chunks of zeros in the high order bits are dropped, with the
 * sixth bit (0x20) used to indicate whether additional chunks follow. These
 * six-bit chunks have a range of 0-63, and are shifted by 63 into the
 * printable character block at 63-126, with characters from the second half
 * at 95-126 making up the body of an encoded number and characters from the
 * first half at 63-94 (for which bit 6 is 0) terminating an encoded number.
 *
 * Results can be checked with:
 * https://developers.google.com/maps/documentation/utilities/polylineutility
 *
 * For comparison see:
 * https://developers.google.com/protocol-buffers/docs/encoding#optional
 * Repeated base-128 varints would be more efficient.
 */

#include "polyline.h"
#include "geometry.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

int encode_double (double c, char *buf) {
    char *b = buf;
    /* 31 == (2^5 - 1) == 00000 00000 00000 00000 00000 11111 */
    uint32_t mask = 31;
    char chunks[6];
    /* initialization to 0 is important so we always get at least
     * one '?' chunk, allowing zero coordinates and deltas
     */
    uint32_t last_chunk = 0;
    uint32_t binary = round(1e5 * c);
    uint8_t i;

    /* printf ("%+10.5f %+10d ", c, binary); */
    /* use the lowest order bit as a sign bit */
    binary <<= 1;
    /* printf ("%+10d ", binary); */
    if ((int32_t)binary < 0) binary = ~binary;
    /* printf ("%+10d ", binary); */
    for (i = 0; i < 6; ++i) {
        chunks[i] = (binary & mask) >> (5 * i);
        /* printf ("%3d ", chunks[i]); */
        /* track the last nonzero chunk. there may be zeros between
         * positive chunks (rendered as '_' == 95)
         */
        if (chunks[i] != 0) last_chunk = i;
        mask <<= 5;
    }
    for (i = 0; i <= last_chunk; ++i) {
        char out = chunks[i];
        /* high bit (range 32-63) indicates another chunk follows */
        if (i != last_chunk) out |= 0x20;
        /* move into alphabetic range 63-126,
         * with 95-126 indicating another chunk follow
         */
        out += 63;
        *(b++) = out;
    }
    *b = '\0';
    /* printf ("%s \n", buf); */
    return (b - buf);
}

/* Our latlon_t contains floats, so results will not be exactly like examples. */
int encode_latlon (latlon_t ll, char *buf) {
    int nc = 0;
    nc += encode_double (ll.lat, buf);
    nc += encode_double (ll.lon, buf + nc);
    return nc;
}

void polyline_begin (polyline_t *pl) {
    pl->last_lat = 0.0;
    pl->last_lon = 0.0;
    pl->buf_cur = pl->buf;
    *pl->buf_cur = '\0';
    pl->n_points = 0;

    /* each latlon could take up to 12 chars */
    pl->buf_max = pl->buf + PL_BUFLEN - 13;
}

/* Allows preserving precision by not using float-based latlon_t. */
void polyline_point (polyline_t *pl, double lat, double lon) {
    double dlat, dlon;

    /* check for potential buffer overflow */
    if (pl->buf_cur >= pl->buf_max) return;

    /* encoded polylines are variable-width,
     * and use coordinate differences to save space.
     */
    dlat = lat - pl->last_lat;
    dlon = lon - pl->last_lon;
    pl->buf_cur += encode_double (dlat, pl->buf_cur);
    pl->buf_cur += encode_double (dlon, pl->buf_cur);
    pl->last_lat = lat;
    pl->last_lon = lon;
    pl->n_points += 1;
}

void polyline_latlon (polyline_t *pl, latlon_t ll) {
    polyline_point (pl, ll.lat, ll.lon);
}

char *polyline_result (polyline_t *pl) {
    return pl->buf;
}

uint32_t polyline_length (polyline_t *pl) {
    return pl->n_points;
}
