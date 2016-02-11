#include "csa_profile.h"

#if 0
conidx_t csa_departures_for_stop (const csa_router_t *router, spidx_t sp_from) {
    conidx_t i_con = router->n_connections;
    conidx_t n = 0;

    while (i_con) {
        i_con--;
        n += (router->connections[i_con].sp_from == sp_from);
    }

    return n;
}
#endif

conidx_t
csa_arrivals_for_stop (const csa_router_t *router, spidx_t sp_to) {
    conidx_t i_con = router->n_connections;
    conidx_t n = 0;

    while (i_con) {
        i_con--;
        n += (router->connections_arrival[i_con].sp_to == sp_to);
    }

    return n;
}

/* Board all upcoming trips in the same block */
void csa_board_backward_profile (csa_router_t *router, conidx_t i_p, vjidx_t vj_index) {
    vehicle_journey_ref_t *interline;

    bitset_set (&router->onboard[i_p], vj_index);

    interline = &router->tdata->vehicle_journey_transfers_backward[vj_index];

    while (interline->jp_index != JP_NONE) {
        vj_index = router->tdata->journey_patterns[interline->jp_index].vj_index + interline->vj_offset;

        bitset_set (&router->onboard[i_p], vj_index);

        interline = &router->tdata->vehicle_journey_transfers_backward[vj_index];
    }
}

static void csa_transfer_profile (csa_router_t *router, router_request_t *req, conidx_t i_p,
                                  conidx_t i_con, spidx_t sp_index_from, rtime_t time_from) {
    uint32_t tr     = router->tdata->stop_points[sp_index_from].transfers_offset;
    uint32_t tr_end = router->tdata->stop_points[sp_index_from + 1].transfers_offset;

    for ( ; tr < tr_end ; ++tr) {
        /* Transfer durations are stored in r_time */
        spidx_t sp_index_to = router->tdata->transfer_target_stops[tr];
        rtime_t transfer_duration = router->tdata->transfer_durations[tr] + req->walk_slack;
        rtime_t time_to = req->arrive_by ? time_from - transfer_duration
                                         : time_from + transfer_duration;

        if ((req->arrive_by ? time_to > router->best_time[i_p + sp_index_to]
                            : time_to < router->best_time[i_p + sp_index_to])) {

            router->best_time[i_p + sp_index_to] = time_to;
            router->states_back_connection[i_p + sp_index_to] = i_con;
        }
    }
}

bool csa_router_profile_arrival (csa_router_t *router, router_request_t *req, conidx_t n) {
    conidx_t t_arr_n = 0;
    conidx_t i_con = 0;

    for (; i_con < router->n_connections; ++i_con) {
        connection_t *con = &router->connections_arrival[i_con];
        conidx_t i_p;

        if (req->to_stop_point == con->sp_to) {
            router->best_time[con->sp_to + t_arr_n] = con->arrival;
            t_arr_n++;
        }

        for (i_p = 0; i_p < t_arr_n; ++i_p) {
            bool is_onboard = bitset_get (&router->onboard[i_p], con->vj_index);
            bool can_board = con->arrival <= router->best_time[i_p + con->sp_to];
            bool improves = con->departure > router->best_time[i_p + con->sp_from];

            if ((is_onboard || can_board) && improves) {
                if (!is_onboard) csa_board_backward_profile (router, i_p, con->vj_index);
                router->best_time[i_p + con->sp_from] = con->departure;
                router->states_back_connection[i_p + con->sp_from] = i_con;

                csa_transfer_profile (router, req, i_p, i_con, con->sp_from, con->departure);
            }
        }
    }

#if 0
    i_con = 0;
    spidx_t i_sp = router->tdata->n_stop_points;
    connection_t *connections = malloc(sizeof (connection_t) * router->tdata->n_stop_points * t_arr_n);
    while (i_sp) {
        i_sp--;
        conidx_t i_p;
        for (i_p = 0; i_p < t_arr_n; ++i_p) {
            connections[i_con].departure = router->best_time[i_p + i_sp];
            connections[i_con].arrival   = router->best_time[i_p + req->to_stop_point];
            connections[i_con].sp_from   = i_sp;
            connections[i_con].sp_to     = req->to_stop_point;

            dump_connection (router->tdata, &connections[i_con]);
            i_con++;
        }
    }

    free (connections);
#endif
    return true;
}

bool csa_profile_arrivals (csa_router_t *router, router_request_t *req) {
    spidx_t sp_to = req->to_stop_point;
    conidx_t n = csa_arrivals_for_stop (router, sp_to);
    fprintf (stderr, "%u stops.\n", router->tdata->n_stop_points);

    fprintf (stderr, "%u arrivals at %s.\n", n,
             tdata_stop_point_name_for_index (router->tdata, sp_to));

    /* all arrivals at sp_to * number of stops to depart from */
    router->best_time = (rtime_t *) malloc (
            sizeof (rtime_t)  * n * router->tdata->n_stop_points);

    router->states_back_connection = (conidx_t *) malloc (
            sizeof (conidx_t) * n * router->tdata->n_stop_points);

    router->onboard = (bitset_t *) malloc (
            sizeof (bitset_t) * n);

    if ( ! (router->best_time &&
            router->states_back_connection
           )
       ) {
        fprintf (stderr, "Failed to allocate router scratch space.\n");
        return false;
    }

    /* TODO: is there a situation that a later trip can't be made seen from
             the perspective of waiting for it at any point? */
    bitset_clear (router->onboard);

    rrrr_memset (router->states_back_connection, CON_NONE, n * router->tdata->n_stop_points);
    rrrr_memset (router->best_time, 0, n * router->tdata->n_stop_points);

    conidx_t i = n;
    while (i) {
        i--;
        bitset_init (&router->onboard[i], router->tdata->n_vjs);
    }

    csa_router_profile_arrival (router, req, n);

    return true;
}

int main(int argc, char *argv[]) {
    router_request_t req;
    csa_router_t router;
    tdata_t tdata;

    memset (&router, 0, sizeof(csa_router_t));
    memset (&tdata, 0, sizeof(tdata_t));

    if ( ! tdata_load (&tdata, argv[1])) {
        goto clean_exit;
    }

    router.tdata = &tdata;

    router_request_initialize (&req);
    router_request_from_epoch (&req, &tdata, strtoepoch("2015-04-13T12:00:00"));
    req.to_stop_point = 1234;

    csa_router_setup_connections (&router, &req);

    csa_profile_arrivals (&router, &req);

clean_exit:
    /* Deallocate the scratchspace of the router */
    csa_router_teardown (&router);
}
