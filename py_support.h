#ifndef _PY_SUPPORT_H
#define _PY_SUPPORT_H

#include    "tdata.h"
#include    "hashgrid.h"	
#include    "geometry.h"
#include    "router.h"


void trans_coord_and_init_grid(tdata_t tdata, HashGrid *hash_grid);

uint32_t evaluate_request(router_t *router, router_request_t *preq, int64_t *best_time, char *result_buf, int round);

char * find_multiple_connections(router_t *router, router_request_t *init_req, int n_conn);


#endif // _PY_SUPPORT_H