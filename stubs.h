#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef uint16_t rtime_t;

typedef struct latlon latlon_t;
struct latlon {
    float lat;
    float lon;
};

typedef struct router_request router_request_t;
struct router_request {
    latlon_t from_latlon;
    latlon_t to_latlon;
    latlon_t via_latlon;
    uint32_t from;
    uint32_t to;
    uint32_t via;
    float walk_slack;
    float walk_speed;
    rtime_t time;
    bool arrive_by;
    bool time_rounded;
};

typedef struct router_t router_t;
struct router_t {
};

typedef struct tdata tdata_t;
struct tdata {
};

bool tdata_load_mmap(tdata_t *tdata, char* filename);
void tdata_close_mmap(tdata_t *tdata);

void router_request_initialize(router_request_t *router);
void router_request_dump(router_t *router, router_request_t *req);
void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime);
bool router_request_reverse(router_t *router, router_request_t *req);

bool router_setup(router_t *router, tdata_t *tdata);
void router_reset(router_t *router);
void router_teardown(router_t *router);
bool router_route(router_t *router, router_request_t *req);

/* return: number of characters written */
uint32_t router_result_dump(router_t *router, router_request_t *req, char *buf, uint32_t buflen);
