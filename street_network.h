#ifndef _STREET_NETWORK_H
#define _STREET_NETWORK_H

#include "rrrr_types.h"
#include "tdata.h"

static bool street_network_mark_entrypoints(router_request_t *req, tdata_t *tdata);
static bool street_network_mark_exitpoints(router_request_t *req, tdata_t *tdata);

#endif
