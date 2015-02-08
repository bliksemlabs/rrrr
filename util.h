#include "rrrr_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <time.h>


#ifndef MIN
#define MIN(a,b) ((a) < (b) ? a : b)
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? a : b)
#endif

#ifndef UNUSED
#define UNUSED(expr) (void)(expr)
#endif

#if defined (HAVE_LOCALTIME_R)
    #define rrrr_localtime_r(a, b) localtime_r(a, b)
#elif defined (HAVE_LOCALTIME_S)
    #define rrrr_localtime_r(a, b) localtime_s(b, a)
#else
    #define rrrr_localtime_r(a, b) { \
    struct tm *tmpstm = localtime (a); \
    memcpy (b, tmpstm, sizeof(struct tm));\
}
#endif

#if defined (HAVE_GMTIME_R)
    #define rrrr_gmtime_r(a, b) gmtime_r(a, b)
#elif defined (HAVE_GMTIME_S)
    #define rrrr_gmtime_r(a, b) gmtime_s(b, a)
#else
    #define rrrr_gmtime_r(a, b) { \
    struct tm *tmpstm = gmtime (a); \
    memcpy (b, tmpstm, sizeof(struct tm));\
}
#endif

#define rrrr_memset(s, u, n) { size_t i = n; do { i--; s[i] = u; } while (i); }

uint32_t rrrrandom(uint32_t limit);
void printBits(size_t const size, void const * const ptr);
rtime_t epoch_to_rtime (time_t epochtime, struct tm *tm_out);
char *btimetext(rtime_t rt, char *buf);
char *timetext(rtime_t t);
time_t strtoepoch (char *time);
char * strcasestr(const char *s, const char *find);

double round(double x);
float median(float *in, uint32_t n, float *min, float *max);
