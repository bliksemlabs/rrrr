#ifndef _ROUTER_REQUEST_H
#define _ROUTER_REQUEST_H

#include "rrrr_types.h"
#include "router.h"
#include "util.h"
#include "config.h"

time_t req_to_date (router_request_t *req, tdata_t *tdata, struct tm *tm_out);
time_t req_to_epoch (router_request_t *req, tdata_t *tdata, struct tm *tm_out);

void router_request_initialize(router_request_t *req);
void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime);
void router_request_randomize (router_request_t *req, tdata_t *tdata);
void router_request_next(router_request_t *req, rtime_t inc);
bool router_request_reverse(router_t *router, router_request_t *req);
bool router_request_reverse_all(router_t *router, router_request_t *req, router_request_t *ret, uint8_t *ret_n);

time_t router_request_to_date (router_request_t *req, tdata_t *tdata, struct tm *tm_out);
time_t router_request_to_epoch (router_request_t *req, tdata_t *tdata, struct tm *tm_out);
bool range_check(router_request_t *req, tdata_t *router);
void router_request_dump(router_request_t *req, tdata_t *tdata);

#endif
