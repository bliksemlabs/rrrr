#include "util.h"
#include "rrrr_types.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

/* buffer should always be at least 13 characters long,
 * including terminating null
 */
char *btimetext(rtime_t rt, char *buf) {
    char *day;
    uint32_t t, s, m, h;

    if (rt == UNREACHED) {
        strcpy(buf, "   --   ");
        return buf;
    }

    if (rt >= RTIME_THREE_DAYS) {
        day = " +2D";
        rt -= RTIME_THREE_DAYS;
    } else if (rt >= RTIME_TWO_DAYS) {
        day = " +1D";
        rt -= RTIME_TWO_DAYS;
    } else if (rt >= RTIME_ONE_DAY) {
        day = "";
        rt -= RTIME_ONE_DAY;
    } else {
        day = " -1D";
    }

    t = RTIME_TO_SEC(rt);
    s = t % 60;
    m = t / 60;
    h = m / 60;
    m = m % 60;
    sprintf(buf, "%02d:%02d:%02d%s", h, m, s, day);

    return buf;
}

uint32_t rrrrandom(uint32_t limit) {
    return (uint32_t) (limit * (rand() / (RAND_MAX + 1.0)));
}

/* Converts the given epoch time to in internal RRRR router time.
 * If epochtime is within the first day of 1970 it is interpreted as seconds
 * since midnight on the current day. If epochtime is 0, the current time and
 * date are used.
 * The intermediate struct tm will be copied to the location pointed to
 * by *stm, unless stm is null. The date should be range checked in the router,
 * where we can see the validity of the tdata file.
 */
rtime_t epoch_to_rtime (time_t epochtime, struct tm *tm_out) {
    struct tm ltm;
    int seconds;
    rtime_t rtime;

    if (epochtime < SEC_IN_ONE_DAY) {
        /* local date and time */
        time_t now;
        time(&now);
        rrrr_localtime_r (&now, &ltm);
        if (epochtime > 0) {
            /* use current date but specified time */
            struct tm etm;
            /* UTC so timezone doesn't affect interpretation of time */
            rrrr_gmtime_r (&epochtime, &etm);
            ltm.tm_hour = etm.tm_hour;
            ltm.tm_min  = etm.tm_min;
            ltm.tm_sec  = etm.tm_sec;
        }
    } else {
        /* use specified time and date */
        rrrr_localtime_r (&epochtime, &ltm);
    }

    if (tm_out != NULL) {
        *tm_out = ltm;
    }

    seconds = (((ltm.tm_hour * 60) + ltm.tm_min) * 60) + ltm.tm_sec;
    rtime = SEC_TO_RTIME(seconds);
    /* shift rtime to day 1. day 0 is yesterday. */
    rtime += RTIME_ONE_DAY;
    #if 0
    fprintf (stderr, "epoch time is %ld \n", epochtime);

    /* ctime and asctime include newlines */
    fprintf (stderr, "epoch time is %s", ctime(&epochtime));
    fprintf (stderr, "ltm is %s", asctime(&ltm));

    fprintf (stderr, "seconds is %d \n", seconds);
    fprintf (stderr, "rtime is %d \n", rtime);
    #endif
    return rtime;
}

#ifdef _XOPEN_SOURCE
time_t strtoepoch (char *time) {
    struct tm ltm;
    memset (&ltm, 0, sizeof(struct tm));
    strptime (time, "%Y-%m-%dT%H:%M:%S", &ltm);
    ltm.tm_isdst = -1;
    return mktime(&ltm);
}
#else
time_t strtoepoch (char *time) {
    char *endptr;
    struct tm ltm;
    memset (&ltm, 0, sizeof(struct tm));
    ltm.tm_year = (int) strtol(time, &endptr, 10) - 1900;
    ltm.tm_mon  = (int) strtol(&endptr[1], &endptr, 10) - 1;
    ltm.tm_mday = (int) strtol(&endptr[1], &endptr, 10);
    ltm.tm_hour = (int) strtol(&endptr[1], &endptr, 10);
    ltm.tm_min  = (int) strtol(&endptr[1], &endptr, 10);
    ltm.tm_sec  = (int) strtol(&endptr[1], &endptr, 10);
    ltm.tm_isdst = -1;
    return mktime(&ltm);
}
#endif

#ifdef RRRR_DEBUG
/* assumes little endian http://stackoverflow.com/a/3974138/778449
 * size in bytes
 */
void printBits(size_t const size, void const * const ptr) {
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    for (i = size - 1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = b[i] & (1 << j);
            byte >>= j;
            fprintf(stderr, "%u", byte);
        }
    }
    puts("");
}
#endif

/* https://answers.yahoo.com/question/index?qid=20091214075728AArnEug */
int compareFloats(const void *elem1, const void *elem2) {
    return ((*((float*) elem1)) - (*((float *) elem2)));
}

/* http://en.wikiversity.org/wiki/C_Source_Code/Find_the_median_and_mean */
float median(float *f, uint32_t n, float *min, float *max) {
    qsort (f, n, sizeof(float), compareFloats);

    if (min) *min = f[0];
    if (max) *max = f[n - 1];

    if((n & 1) == 0) {
        /* even number of elements, return the mean of the two elements */
        return ((f[(n >> 1)] + f[(n >> 1) - 1]) / 2.0f);
    } else {
        /* else return the element in the middle */
        return f[n >> 1];
    }
}

