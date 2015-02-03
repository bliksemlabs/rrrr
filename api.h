#include "config.h"
#include "router_result.h"
#include "router_request.h"

bool router_route_first_departure (router_t *router, router_request_t *req, plan_t *plan);
bool router_route_naive_reversal (router_t *router, router_request_t *req, plan_t *plan);
bool router_route_full_reversal (router_t *router, router_request_t *req, plan_t *plan);
