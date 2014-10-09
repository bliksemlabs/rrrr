#ifndef _STUBS_H
#define _STUBS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "rrrr_types.h"

#ifdef HAVE_LOCALTIME_R
    #define rrrr_localtime_r(a, b) localtime_r(a, b)
#elif HAVE_LOCALTIME_S
    #define rrrr_localtime_r(a, b) localtime_s(b, a)
#else
    #define rrrr_localtime_r(a, b) { \
    struct tm *tmpstm = localtime (a); \
    memcpy (b, tmpstm, sizeof(struct tm));\
}
#endif

#include "tdata.h"
#include "router_request.h"

void tdata_close_mmap(tdata_t *tdata);

/* return: number of characters written */
uint32_t router_result_dump(router_t *router, router_request_t *req, char *buf, uint32_t buflen);

void memset32(uint32_t *s, uint32_t u, size_t n);
char * strcasestr(const char *s, const char *find);
rtime_t epoch_to_rtime (time_t epochtime, struct tm *localtm);
uint32_t rrrrandom(uint32_t limit);

char *tdata_stop_name_for_index(tdata_t *td, uint32_t stop_index);
char *btimetext(rtime_t t, char *buf);

#ifdef RRRR_FEATURE_AGENCY_FILTER
uint32_t rrrrandom_stop_by_agency(tdata_t *tdata, uint16_t agency_index);
#endif

rtime_t epoch_to_rtime (time_t epochtime, struct tm *localtm);

void printBits(size_t const size, void const * const ptr);

#endif

