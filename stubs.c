#include "stubs.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

bool tdata_load(tdata_t *tdata, char* filename) { return true; }
void tdata_close(tdata_t *tdata) {}

void router_request_initialize(router_request_t *router) {}
void router_request_dump(router_t *router, router_request_t *req) {}
void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime) {}
bool router_request_reverse(router_t *router, router_request_t *req) { return true; }

bool router_setup(router_t* router, tdata_t* td) { return true; }
void router_reset(router_t *router) {}
void router_teardown(router_t *router) {}
bool router_route(router_t *router, router_request_t *req) { return true; }

/* return: number of characters written */
uint32_t router_result_dump(router_t *router, router_request_t *req, char *buf, uint32_t buflen) { return 0; }

char *btimetext(rtime_t rt, char *buf) { return ""; }

rtime_t epoch_to_rtime (time_t epochtime, struct tm *localtm) { return 0; }

