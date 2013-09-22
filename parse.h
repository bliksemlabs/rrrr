#include "router.h"
#include "tdata.h"

void parse_request(router_request_t *req, tdata_t *tdata, int opt, char *optarg);

bool parse_request_from_qstring(router_request_t*, tdata_t *tdata);
