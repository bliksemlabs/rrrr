#ifndef _RRRR_TYPES_H
#define _RRRR_TYPES_H

/* 2^16 / 60 / 60 is 18.2 hours at one-second resolution.
 * By right-shifting times one bit, we get 36.4 hours (over 1.5 days) at 2 second resolution.
 * By right-shifting times two bits, we get 72.8 hours (over 3 days) at 4 second resolution.
 * Three days is just enough to model yesterday, today, and tomorrow for overnight searches,
 * and can also represent the longest rail journeys in Europe.
*/
typedef uint16_t rtime_t;

#ifdef RRRR_FEATURE_REALTIME_EXPANDED
typedef struct list list_t;
struct list {
    void *list;
    uint32_t size;
    uint32_t len;
};
#endif

#ifndef _LP64
    #define ZU "%u"
#else
    #define ZU "%lu"
#endif

#endif /* _RRRR_TYPES */
