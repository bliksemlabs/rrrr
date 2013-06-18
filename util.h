/* util.h */

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <stdlib.h>

// BTW why am I not using unsigned ints (uint32_t)

// 2**16 / 60 / 60 is 18 hours
// by right-shifting all times one bit we get 36 hours (1.5 days) at 2 second resolution
typedef uint16_t rtime_t;

// UNREACHED must be bigger than any time used in the router for the algorithm to work right
// Ideally we should never rely on the numerical value of these preprocessor constants
// since they will be used in both departAfter and arriveBy searches.
#define UNREACHED UINT16_MAX
#define NONE -1

void die(const char* message);

char *btimetext(rtime_t t, char *buf, int buflen);

char *timetext(rtime_t t);

void printBits(size_t const size, void const * const ptr);

#endif // _UTIL_H
