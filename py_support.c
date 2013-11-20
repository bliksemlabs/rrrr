#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#include "py_support.h"
#include "json.h"
#include "router.h"



#define OUTPUT_LEN 128000

static int64_t rtime_to_msec(rtime_t rtime, time_t date) { return (RTIME_TO_SEC_SIGNED(rtime - RTIME_ONE_DAY) + date) * 1000L; }


void trans_coord_and_init_grid(tdata_t tdata, HashGrid *hash_grid)
{
	coord_t coords[tdata.n_stops];
    for (uint32_t c = 0; c < tdata.n_stops; ++c)
        coord_from_latlon(coords + c, tdata.stop_coords + c);

    HashGrid_init (hash_grid, 100, 500.0, coords, tdata.n_stops);
}


char * find_multiple_connections(router_t *router, router_request_t *init_req, int n_conn)
{
	char *result_buf = (char *) malloc(OUTPUT_LEN);
	int32_t current_lenght = 0;
	int64_t best_time;

	for (int i=0; i<n_conn; i++){
		if (i != 0){
			result_buf[current_lenght - 3] = ','; // coma has to be added between objects in itineraries array
			result_buf[current_lenght - 2] = '\0'; // extra brackets closing objects has to be removed
			current_lenght -= 2;
			result_buf = realloc(result_buf, current_lenght + OUTPUT_LEN); // resize needed
		}
		if (init_req->arrive_by)
		{
			current_lenght += evaluate_request(router, init_req, &best_time, result_buf + current_lenght, i);
			router_request_from_epoch (init_req, router->tdata, best_time - 5); // recreate request with new start time.
		}else
		{
			current_lenght += evaluate_request(router, init_req, &best_time, result_buf + current_lenght, i);
			router_request_from_epoch (init_req, router->tdata, best_time + 5); // recreate request with new start time.
		}
	}
	result_buf = realloc(result_buf, current_lenght + 1);
	return result_buf;
}


uint32_t evaluate_request(router_t *router, router_request_t *preq, int64_t *best_time, char *result_buf, int round)
{
	router_request_t req = *preq; // protective copy, since we're going to reverse it
	uint32_t result_length;
	int verbose = 1;

	if (verbose){
		printf ("Searching with request: \n");
		router_request_dump (router, &req);
	}

	router_route (router, &req);
	// repeat search in reverse to compact transfers
	uint32_t n_reversals = req.arrive_by ? 1 : 2;
	//n_reversals = 0; // DEBUG turn off reversals
	for (uint32_t i = 0; i < n_reversals; ++i) {
		router_request_reverse (router, &req); // handle case where route is not reversed
		router_route (router, &req);
	}

	if (verbose){
		router_result_dump(router, &req, result_buf, OUTPUT_LEN);
	    printf("%s", result_buf);
	}

	struct plan plan;
	router_result_to_plan (&plan, router, &req);
	plan.req.time = preq->time; // restore the original request time
	if (round == 0)
		result_length = render_plan_json (&plan, router->tdata, result_buf, OUTPUT_LEN);
	else
		result_length = render_itin_json_only (&plan, router->tdata, result_buf, OUTPUT_LEN);

	if (best_time != NULL){
		struct tm ltm;
		time_t date_seconds = req_to_date(& plan.req, router->tdata, &ltm);

		if (preq->arrive_by) // if true, than we need the arrival time. that is t1 of all legs.
			*best_time = rtime_to_msec(plan.itineraries[plan.n_itineraries - 1].legs[plan.itineraries[0].n_legs - 1].t1, date_seconds) / 1000; // not sure whether there might be multiple itineraries
		else
			*best_time = rtime_to_msec(plan.itineraries[0].legs[0].t0, date_seconds) / 1000;
	}
	return result_length;
}