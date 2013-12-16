/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

#include "router.h"
#include "tdata.h"
#include "hashgrid.h"

void parse_request(router_request_t *req, tdata_t *tdata, HashGrid *hg, int opt, char *optarg);

bool parse_request_from_qstring(router_request_t*, tdata_t *tdata, HashGrid *hg, char *qstring);
