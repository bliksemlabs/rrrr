/* util.h */

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
/*
  2^16 / 60 / 60 is 18.2 hours at one-second resolution.
  By right-shifting times one bit, we get 36.4 hours (over 1.5 days) at 2 second resolution.
  By right-shifting times two bits, we get 72.8 hours (over 3 days) at 4 second resolution.
  Three days is just enough to model yesterday, today, and tomorrow for overnight searches,
  and can also represent the longest rail journeys in Europe.
*/
typedef uint16_t rtime_t;

#define SEC_TO_RTIME(x) ((x) >> 2)
#define RTIME_TO_SEC(x) (((uint32_t)x) << 2)
#define RTIME_TO_SEC_SIGNED(x) ((x) << 2)

#define SEC_IN_ONE_DAY    (24 * 60 * 60)
#define SEC_IN_TWO_DAYS   (2 * SEC_IN_ONE_DAY)
#define SEC_IN_THREE_DAYS (3 * SEC_IN_ONE_DAY)
#define RTIME_ONE_DAY     (SEC_TO_RTIME(SEC_IN_ONE_DAY))
#define RTIME_TWO_DAYS    (SEC_TO_RTIME(SEC_IN_TWO_DAYS))
#define RTIME_THREE_DAYS  (SEC_TO_RTIME(SEC_IN_THREE_DAYS))

// We should avoid relying on the relative value of these preprocessor constants (inequalities)
// since they will be used in both departAfter and arriveBy searches.
#define UNREACHED UINT16_MAX
#define NONE      (UINT32_MAX)
#define WALK      (UINT32_MAX - 1)
#define ONBOARD   (UINT32_MAX - 2)
#define CANCELED  INT16_MAX

void die(const char* message);

char *btimetext(rtime_t t, char *buf); // minimum buffer size is 9 characters

char *timetext(rtime_t t);

void printBits(size_t const size, void const * const ptr);

rtime_t epoch_to_rtime (time_t epochtime, struct tm *localtm);

#endif // _UTIL_H
