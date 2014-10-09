#include "stubs.h"

bool tdata_load_mmap(tdata_t *tdata, char* filename) { return true; }
void tdata_close_mmap(tdata_t *tdata) {}

void router_request_initialize(router_request_t *router) {}
void router_request_dump(router_t *router, router_request_t *req) {}
void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime) {}
bool router_request_reverse(router_t *router, router_request_t *req) { return true; }

void router_reset(router_t *router) {}
void router_teardown(router_t *router) {}
bool router_route(router_t *router, router_request_t *req) { return true; }

/* return: number of characters written */
uint32_t router_result_dump(router_t *router, router_request_t *req, char *buf, uint32_t buflen) { return 0; }

void memset32(uint32_t *s, uint32_t u, size_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        s[i] = u;
    }
}

uint32_t rrrrandom(uint32_t limit) {
    return (uint32_t) (limit * (random() / (RAND_MAX + 1.0)));
}

char *btimetext(rtime_t rt, char *buf) { return ""; }

rtime_t epoch_to_rtime (time_t epochtime, struct tm *localtm) { return 0; }


/*
  Check the given request against the characteristics of the router that will be used.
  Indexes larger than array lengths for the given router, signed values less than zero, etc.
  can and will cause segfaults and present security risks.

  We could also infer departure stop etc. from start trip here, "missing start point" and reversal problems.
*/
bool range_check(struct router_request *req, struct router *router) {
    uint32_t n_stops = router->tdata.n_stops;
    if (req->time < 0)         return false;
    if (req->walk_speed < 0.1) return false;
    if (req->from >= n_stops)  return false;
    if (req->to >= n_stops)    return false;
    return true;
}

/* assumes little endian http://stackoverflow.com/a/3974138/778449
 * size in bytes
 */
void printBits(size_t const size, void const * const ptr) {
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    for (i = size-1; i>= 0; i--) {
        for (j=7; j>=0; j--) {
            byte = b[i] & (1<<j);
            byte >>= j;
            printf("%u", byte);
        }
    }
    puts("");
}

