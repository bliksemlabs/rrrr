#include "router_dump.h"
#include "router.h"
#include "stubs.h"
#include "tdata.h"
#include "config.h"

#include <stdio.h>

void router_state_dump (router_state_t *state) {
    char walk_time[13], time[13], board_time[13];
    printf ("-- Router State --\n"
            "walk time:    %s\n"
            "walk from:    %d\n"
            "time:         %s\n"
            "board time:   %s\n"
            "back route:   ",
            btimetext(state->walk_time, walk_time),
            state->walk_from,
            btimetext(state->time, time),
            btimetext(state->board_time, board_time)
            );

    /* TODO */
    if (state->back_route == NONE) printf ("NONE\n");
    else printf ("%d\n", state->back_route);
}

bool stop_is_reached(router_t *router, uint32_t stop_index) {
    uint8_t i_round;
    bool ret_reached = false;

    for (i_round = 0; i_round < RRRR_DEFAULT_MAX_ROUNDS; ++i_round) {
        if (router->states[i_round * router->tdata->n_stops + stop_index].walk_time != UNREACHED) {
            ret_reached = true;
            break;
        }
    }
    return ret_reached;
}

void dump_results(router_t *router) {
    uint32_t i_round, i_stop;
    #if 0
    char id_fmt[10];
    sprintf(id_fmt, "%%%ds", router.tdata.stop_id_width);
    #else
    char *id_fmt = "%30.30s";
    #endif

    printf("\nRouter states:\n");
    printf(id_fmt, "Stop name");
    printf(" [sindex]");

    for (i_round = 0; i_round < RRRR_DEFAULT_MAX_ROUNDS; ++i_round) {
        printf("  round %d   walk %d", i_round, i_round);
    }
    printf("\n");

    for (i_stop = 0; i_stop < router->tdata->n_stops; ++i_stop) {
        char *stop_id;
        char time[13], walk_time[13];

        /* filter out stops which will not be reached */
        if (! stop_is_reached(router, i_stop)) continue;

        stop_id = tdata_stop_name_for_index (router->tdata, i_stop);
        printf(id_fmt, stop_id);
        printf(" [%6d]", i_stop);
        for (i_round = 0; i_round < RRRR_DEFAULT_MAX_ROUNDS; ++i_round) {
            printf(" %8s %8s",
                btimetext(router->states[i_round * router->tdata->n_stops + i_stop].time, time),
                btimetext(router->states[i_round * router->tdata->n_stops + i_stop].walk_time, walk_time));
        }
        printf("\n");
    }
    printf("\n");
}

#if 0
/* WARNING we are not currently storing trip IDs so this will segfault */
void dump_trips(router_t *router) {
    uint32_t i_route;
    for (i_route = 0; i_route < router->tdata->n_routes; ++i_route) {
        route_t *route = &(router->tdata->routes[i_route]);
        char *trip_ids = tdata_trip_ids_for_route(router->tdata, i_route);
        uint32_t *trip_masks = tdata_trip_masks_for_route(router->tdata, i_route);
        uint32_t i_trip;

        printf ("route %d (of %d), n trips %d, n stops %d\n", i_route, router->tdata->n_routes, route->n_trips, route->n_stops);
        for (i_trip = 0; i_trip < route->n_trips; ++i_trip) {
            printf ("trip index %d trip_id %s mask ", i_trip, trip_ids[i_trip * router->tdata->trip_id_width]);
            printBits (4, & (trip_masks[i_trip]));
            printf ("\n");
        }
    }
}
#endif

void day_mask_dump (uint32_t mask) {
    uint8_t i_bit;
    printf ("day mask: ");
    printBits (4, &mask);
    printf ("bits set: ");

    for (i_bit = 0; i_bit < 32; ++i_bit)
        if (mask & (1 << i_bit))
            printf ("%d ", i_bit);

    printf ("\n");
}

void service_day_dump (struct service_day *sd) {
    char midnight[13];
    printf ("service day\nmidnight: %s \n", btimetext(sd->midnight, midnight));
    day_mask_dump (sd->mask);
    printf ("real-time: %s \n\n", sd->apply_realtime ? "YES" : "NO");
}

