#include "config.h"
#include "router_dump.h"
#include "router.h"
#include "util.h"
#include "tdata.h"

#include <stdio.h>

void router_state_dump (router_state_t *state) {
    char walk_time[13], time[13], board_time[13];
    fprintf (stderr, "-- Router State --\n"
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
    if (state->back_journey_pattern == NONE) fprintf (stderr, "NONE\n");
    else fprintf (stderr, "%d\n", state->back_journey_pattern);
}

void dump_results(router_t *router) {
    uint32_t i_round, i_stop;
    #if 0
    char id_fmt[10];
    sprintf(id_fmt, "%%%ds", router.tdata.stop_id_width);
    #else
    char *id_fmt = "%30.30s";
    #endif

    fprintf(stderr, "\nRouter states:\n");
    fprintf(stderr, id_fmt, "Stop name");
    fprintf(stderr, " [sindex]");

    for (i_round = 0; i_round < RRRR_DEFAULT_MAX_ROUNDS; ++i_round) {
        fprintf(stderr, "  round %d   walk %d", i_round, i_round);
    }
    fprintf(stderr, "\n");

    for (i_stop = 0; i_stop < router->tdata->n_stops; ++i_stop) {
        const char *stop_id;
        char time[13], walk_time[13];

        /* filter out stops which will not be reached */
        if (router->best_time[i_stop] == UNREACHED) continue;

        stop_id = tdata_stop_name_for_index (router->tdata, i_stop);
        fprintf(stderr, id_fmt, stop_id);
        fprintf(stderr, " [%6d]", i_stop);
        for (i_round = 0; i_round < RRRR_DEFAULT_MAX_ROUNDS; ++i_round) {
            fprintf(stderr, " %8s %8s",
                btimetext(router->states[i_round * router->tdata->n_stops + i_stop].time, time),
                btimetext(router->states[i_round * router->tdata->n_stops + i_stop].walk_time, walk_time));
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
}

#if 0
/* WARNING we are not currently storing trip IDs so this will segfault */
void dump_trips(router_t *router) {
    uint32_t jp_index;
    for (jp_index = 0; jp_index < router->tdata->n_journey_patterns; ++jp_index) {
        journey_pattern_t *jp = &(router->tdata->journey_patterns[jp_index]);
        char *trip_ids = tdata_trip_ids_in_journey_pattern(router->tdata, jp_index);
        uint32_t *trip_masks = tdata_trip_masks_for_journey_pattern(router->tdata, jp_index);
        uint32_t i_trip;

        printf ("journey_pattern %d (of %d), n trips %d, n stops %d\n", jp_index, router->tdata->n_journey_patterns, jp->n_trips, jp->n_stops);
        for (i_trip = 0; i_trip < jp->n_trips; ++i_trip) {
            printf ("trip index %d trip_id %s mask ", i_trip, trip_ids[i_trip * router->tdata->trip_ids_width]);
            printBits (4, & (trip_masks[i_trip]));
            printf ("\n");
        }
    }
}
#endif

#ifdef RRRR_DEBUG
void day_mask_dump (uint32_t mask) {
    uint8_t i_bit;
    fprintf (stderr, "day mask: ");
    printBits (4, &mask);
    fprintf (stderr, "bits set: ");

    for (i_bit = 0; i_bit < 32; ++i_bit)
        if (mask & (1 << i_bit))
            fprintf (stderr, "%d ", i_bit);

    fprintf (stderr, "\n");
}

void service_day_dump (struct service_day *sd) {
    char midnight[13];
    fprintf (stderr, "service day\nmidnight: %s \n",
                      btimetext(sd->midnight, midnight));

    day_mask_dump (sd->mask);
    fprintf (stderr, "real-time: %s \n\n", sd->apply_realtime ? "YES" : "NO");
}
#endif
