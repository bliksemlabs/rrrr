#include "expand_vehicle_journeys.h"

typedef struct csa_profile csa_profile_t;
struct csa_profile {
    /* aankomsttijd op p_target, als index */
    rtime_t t_arr;

    /* laatst mogelijke vertrektijd vanaf deze halte */
    rtime_t t_dep;

    /* hoe zijn we hier gekomen? */
    conidx_t con;
};

conidx_t csa_arrivals_for_stop   (const csa_router_t *router, spidx_t sp_to);
conidx_t csa_departures_for_stop (const csa_router_t *router, spidx_t sp_from);


