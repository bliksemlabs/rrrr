/* util.h */

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <stdlib.h>

// 2**16 / 60 / 60 is 18.2 hours at one-second resolution.
// by right-shifting times one bit, we get 36.4 hours (over 1.5 days) at 2 second resolution.
// by right-shifting times two bits, we get 72.8 hours (over 3 days) at 4 second resolution.
// Three days is just enough to model yesterday, today, and tomorrow for overnight searches,
// and can also represent the longest rail journeys in Europe.
typedef uint16_t rtime_t;

#define SEC_TO_RTIME(x) (x >> 1)
#define RTIME_TO_SEC(x) (x << 1)
#define RTIME_ONE_DAY (RTIME_FROM_SEC(24 * 60 * 60))

// We should avoid relying on the relative value of these preprocessor constants (inequalities)
// since they will be used in both departAfter and arriveBy searches.
#define UNREACHED UINT16_MAX
#define NONE (UINT32_MAX)
#define WALK (UINT32_MAX - 1)

void die(const char* message);

char *btimetext(rtime_t t, char *buf); // minimum buffer size is 9 characters

char *timetext(rtime_t t);

void printBits(size_t const size, void const * const ptr);

#endif // _UTIL_H
