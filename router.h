/* router.h */
#include <stdbool.h>
#include "transitdata.h"

typedef struct router_state router_state_t;
struct router_state {
    int arrival_time;
    int back_stop;
    int back_route;
    int board_time;
    char *back_trip_id;
};

typedef struct router router_t;
struct router {
    transit_data_t tdata;
    // scratch space for solving
    // making this opaque requires more dynamic allocation
    int table_size;
    int *best_time;
    router_state_t *states;
    // maybe we should store more routing state in here, like round and sub-scratch pointers
};

typedef struct router_request router_request_t;
struct router_request {
    int from;
    int to;
    long time;
    double walk_speed;
    bool arrive_by; 
};

void router_setup(router_t*, transit_data_t*);

bool router_request_from_qstring(router_request_t*);

void router_request_dump(router_t*, router_request_t*);

void router_request_randomize(router_request_t*);

void router_teardown(router_t*);

bool router_route(router_t*, router_request_t*);

int router_result_dump(router_t*, router_request_t*, char *buf, int buflen); // return num of chars written
