/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

#include <math.h>
#define deg2rad(d) ((d*M_PI)/180.0f)
#define rad2deg(d) ((d*180.0f)/M_PI)
#define earth_radius 6378137.0f

/* The following functions take or return there results in degrees */

GLfloat y2lat_d(GLfloat y) { return rad2deg(2.0f * atanf(expf(deg2rad(y) ) ) - M_PI/2.0f); }
GLfloat x2lon_d(GLfloat x) { return x; }
GLfloat lat2y_d(GLfloat lat) { return rad2deg(logf(tanf(M_PI/4.0f+ deg2rad(lat)/2.0f))); }
GLfloat lon2x_d(GLfloat lon) { return lon; }

/* The following functions take or return there results in something close to meters, along the equator */

GLfloat y2lat_m(GLfloat y) { return rad2deg(2.0f * atanf(expf( (y / earth_radius ) ) - M_PI/2.0f)); }
GLfloat x2lon_m(GLfloat x) { return rad2deg(x / earth_radius); }
GLfloat lat2y_m(GLfloat lat) { return earth_radius * logf(tanf(M_PI/4.0f+ deg2rad(lat)/2.0f)); }
GLfloat lon2x_m(GLfloat lon) { return deg2rad(lon) * earth_radius; }
