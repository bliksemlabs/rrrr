#include "rrrr_types.h"
#include "tdata.h"
#include "plan.h"
#include "router_request.h"

#define METADATA 1

typedef struct connection connection_t;
struct connection {
    rtime_t departure;
    rtime_t arrival;
    spidx_t sp_from;
    spidx_t sp_to;

#ifdef METADATA
    jpidx_t journey_pattern;
    jp_vjoffset_t vehicle_journey;
#endif
};

typedef uint32_t conidx_t;
#define CON_NONE   ((conidx_t) -1)

typedef struct csa_router csa_router_t;
struct csa_router {
    /* The transit / timetable data tables */
    tdata_t *tdata;

    /* The best known time at each stop_point */
    rtime_t *best_time;

    /* The index of the connection used to travel from back_stop_point */
    conidx_t *states_back_connection;

    /* The number of connections */
    conidx_t n_connections;

    /* The computed connections */
    connection_t *connections_departure;
    connection_t *connections_arrival;
};

bool expand_vehicle_journeys (const tdata_t *td, router_request_t *req,
                              connection_t *connections, conidx_t n_connections);

bool csa_router_setup (csa_router_t *router, tdata_t *tdata);
void csa_router_teardown (csa_router_t *router);

bool csa_router_setup_connections (csa_router_t *router, router_request_t *req);
void csa_router_teardown_connections (csa_router_t *router);

bool csa_router_route_departure (csa_router_t *router, router_request_t *req);
bool csa_router_route_arrival (csa_router_t *router, router_request_t *req);
bool csa_router_result_to_plan (plan_t *plan, csa_router_t *router, router_request_t *req);

