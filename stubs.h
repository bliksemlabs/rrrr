#ifndef _STUBS_H
#define _STUBS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "rrrr_types.h"

#define SEC_TO_RTIME(x) ((x) >> 2)
#define RTIME_TO_SEC(x) (((uint32_t)x) << 2)
#define RTIME_TO_SEC_SIGNED(x) ((x) << 2)

#define SEC_IN_ONE_MINUTE (60)
#define SEC_IN_ONE_HOUR   (60 * SEC_IN_ONE_MINUTE)
#define SEC_IN_ONE_DAY    (24 * SEC_IN_ONE_HOUR)
#define SEC_IN_TWO_DAYS   (2 * SEC_IN_ONE_DAY)
#define SEC_IN_THREE_DAYS (3 * SEC_IN_ONE_DAY)
#define RTIME_ONE_DAY     (SEC_TO_RTIME(SEC_IN_ONE_DAY))
#define RTIME_TWO_DAYS    (SEC_TO_RTIME(SEC_IN_TWO_DAYS))
#define RTIME_THREE_DAYS  (SEC_TO_RTIME(SEC_IN_THREE_DAYS))

#define UNREACHED UINT16_MAX
#define NONE      (UINT32_MAX)
#define WALK      (UINT32_MAX - 1)
#define ONBOARD   (UINT32_MAX - 2)

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

typedef enum tmode {
    m_tram      =   1,
    m_subway    =   2,
    m_rail      =   4,
    m_bus       =   8,
    m_ferry     =  16,
    m_cablecar  =  32,
    m_gondola   =  64,
    m_funicular = 128,
    m_all       = 255
} tmode_t;

typedef enum optimise {
    /* shortest time */
    o_shortest  =   1,
    /* least transfers */
    o_transfers =   2,
    /* everything */
    o_all       = 255
} optimise_t;

typedef enum trip_attributes {
    ta_none = 0,
    ta_accessible = 1,
    ta_toilet = 2,
    ta_wifi = 4
} trip_attributes_t;

typedef struct router_state router_state_t;
struct router_state {
    rtime_t  walk_time;
};

#include "tdata.h"

typedef struct router_t router_t;
struct router_t {
    tdata_t *tdata;
    router_state_t *states;
};

#include "router_request.h"

void tdata_close_mmap(tdata_t *tdata);

bool router_setup(router_t *router, tdata_t *tdata);
void router_reset(router_t *router);
void router_teardown(router_t *router);
bool router_route(router_t *router, router_request_t *req);

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

#endif

