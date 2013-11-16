/* router.c : the main routing algorithm */
#include "router.h" // first to ensure it works alone

#include "util.h"
#include "config.h"
#include "tdata.h"
#include "bitset.h"
#include "json.h"
#include "parse.h"
#include "polyline.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define RRRR_RESULT_BUFLEN 16000
static char result_buf[RRRR_RESULT_BUFLEN];

void router_setup(router_t *router, tdata_t *tdata) {
    srand(time(NULL));
    router->tdata = tdata;
    router->best_time = (rtime_t *) malloc(sizeof(rtime_t) * tdata->n_stops); 
    router->states = (router_state_t *) malloc(sizeof(router_state_t) * (tdata->n_stops * RRRR_MAX_ROUNDS));
    router->updated_stops = bitset_new(tdata->n_stops);
    router->updated_routes = bitset_new(tdata->n_routes);
    if ( ! (router->best_time && router->states && router->updated_stops && router->updated_routes))
        die("failed to allocate router scratch space");
}

static inline void router_reset(router_t router) {

}

void router_teardown(router_t *router) {
    free(router->best_time);
    free(router->states);
    bitset_destroy(router->updated_stops);
    bitset_destroy(router->updated_routes);
}

// TODO? flag_routes_for_stops all at once after doing transfers? this would require another stops 
// bitset for transfer target stops.
/* Given a stop index, mark all routes that serve it as updated. */
static inline void flag_routes_for_stop (router_t *router, router_request_t *req, uint32_t stop_index) {
    uint32_t *routes;
    uint32_t n_routes = tdata_routes_for_stop (router->tdata, stop_index, &routes);
    for (uint32_t i = 0; i < n_routes; ++i) {
        calendar_t route_active_flags = router->tdata->route_active[routes[i]];
        I printf ("  flagging route %d at stop %d\n", routes[i], stop_index);
        // CHECK that there are any trips running on this route (another bitfield)
        // printf("route flags %d", route_active_flags);
        // printBits(4, &route_active_flags);
        if ((router->day_mask & route_active_flags) && // seems to provide about 14% increase in throughput
            (req->mode & router->tdata->routes[routes[i]].attributes) > 0) {
           bitset_set (router->updated_routes, routes[i]);
           I printf ("  route running\n");
        }
    }
}

static inline void unflag_banned_routes (router_t *router, router_request_t *req) {
     for (uint32_t i = 0; i < req->n_banned_routes; ++i) {
         bitset_unset (router->updated_routes, req->banned_route);
     }
}

static inline void unflag_banned_stops (router_t *router, router_request_t *req) {
     for (uint32_t i = 0; i < req->n_banned_stops; ++i) {
         bitset_unset (router->updated_stops, req->banned_stop);
     }
}

/* Because the first round begins with so few reached stops, the initial state doesn't get its own full array of states. 
   Instead we reuse one of the later rounds (round 1) for the initial state. This means we need to reset the walks in
   round 1 back to UNREACHED before using them in routing. Rather than iterating over all of them, we only initialize
   the stops that can be reached by transfers.
   Alternatively we could instead initialize walks to UNREACHED at the beginning of the transfer calculation function. 
   We should not however reset the best times for those stops reached from the initial stop on foot. This will prevent 
   finding circuitous itineraries that return to them.
   */
static inline void initialize_transfers (router_t *router, uint32_t round, uint32_t stop_index_from) {
    router_state_t *states = router->states + (round * router->tdata->n_stops);
    states[stop_index_from].walk_time = UNREACHED;
    uint32_t t  = router->tdata->stops[stop_index_from    ].transfers_offset;
    uint32_t tN = router->tdata->stops[stop_index_from + 1].transfers_offset;        
    for ( ; t < tN ; ++t) {
        uint32_t stop_index_to = router->tdata->transfer_target_stops[t];
        states[stop_index_to].walk_time = UNREACHED;
    }
}

/* Rather than reserving a place to store the transfers used to create the initial state, we look them up as needed. */
static inline rtime_t 
transfer_duration (tdata_t *tdata, router_request_t *req, uint32_t stop_index_from, uint32_t stop_index_to) {
    if (stop_index_from == stop_index_to) return 0;
    uint32_t t  = tdata->stops[stop_index_from    ].transfers_offset;
    uint32_t tN = tdata->stops[stop_index_from + 1].transfers_offset;        
    for ( ; t < tN ; ++t) {
        if (tdata->transfer_target_stops[t] == stop_index_to) {
            uint32_t distance_meters = tdata->transfer_dist_meters[t] << 4; // actually in units of 16 meters
            return SEC_TO_RTIME((uint32_t)(distance_meters / req->walk_speed + req->walk_slack));
        }
    }
    return UNREACHED;
}

uint32_t
transfer_distance (tdata_t *tdata, uint32_t stop_index_from, uint32_t stop_index_to) {
    if (stop_index_from == stop_index_to) return 0;
    uint32_t t  = tdata->stops[stop_index_from    ].transfers_offset;
    uint32_t tN = tdata->stops[stop_index_from + 1].transfers_offset;        
    for ( ; t < tN ; ++t) {
        if (tdata->transfer_target_stops[t] == stop_index_to) {
            return tdata->transfer_dist_meters[t] << 4; // actually in units of 16 meters
        }
    }
    return UNREACHED;
}


/* 
 For each updated stop and each destination of a transfer from an updated stop, 
 set the associated routes as updated. The routes bitset is cleared before the operation, 
 and the stops bitset is cleared after all transfers have been computed and all routes have been set.
 Transfer results are computed within the same round, based on arrival time in the ride phase and
 stored in the walk time member of states.
*/
static inline void 
apply_transfers (router_t *router, router_request_t *req, uint32_t round) {
    router_state_t *states = router->states + (round * router->tdata->n_stops);
    /* The transfer process will flag routes that should be explored in the next round */
    bitset_reset (router->updated_routes);
    for (uint32_t stop_index_from  = bitset_next_set_bit (router->updated_stops, 0); 
                  stop_index_from != BITSET_NONE;
                  stop_index_from  = bitset_next_set_bit (router->updated_stops, stop_index_from + 1)) {
        I printf ("stop %d was marked as updated \n", stop_index_from);
        router_state_t *state_from = states + stop_index_from;
        rtime_t time_from = state_from->time;
        if (time_from == UNREACHED) {
            printf ("ERROR: transferring from unreached stop %d in round %d. \n", stop_index_from, round);
            continue;
        }
        /* At this point, the best time at the from stop may be different than the state_from->time,
           because the best time may have been updated by a transfer. */
        /*   
        if (time_from != router->best_time[stop_index_from]) {            
            printf ("ERROR: time at stop %d in round %d is not the same as its best time. \n", stop_index_from, round);
            printf ("    from time %s \n", timetext(time_from));
            printf ("    walk time %s \n", timetext(state_from->walk_time));
            printf ("    best time %s \n", timetext(router->best_time[stop_index_from]));
            continue;
        }
        */
        I printf ("  applying transfer at %d (%s) \n", stop_index_from, tdata_stop_name_for_index(router->tdata, stop_index_from));
        /* First apply a transfer from the stop to itself, if case that's the best way */
        if (state_from->time == router->best_time[stop_index_from]) { 
            /* This state's best time is still its own. No improvements from other transfers. */
            state_from->walk_time = time_from; 
            state_from->walk_from = stop_index_from;
            // assert (router->best_time[stop_index_from] == time_from);
            flag_routes_for_stop (router, req, stop_index_from);
            unflag_banned_routes (router, req);
        }
        /* Then apply transfers from the stop to nearby stops */
        uint32_t tr     = router->tdata->stops[stop_index_from    ].transfers_offset;
        uint32_t tr_end = router->tdata->stops[stop_index_from + 1].transfers_offset;        
        for ( ; tr < tr_end ; ++tr) {
            uint32_t stop_index_to = router->tdata->transfer_target_stops[tr];
            /* Transfer distances are stored in units of 16 meters, rounded not truncated, in a uint8_t */
            uint32_t dist_meters = router->tdata->transfer_dist_meters[tr] << 4; 
            rtime_t transfer_duration = SEC_TO_RTIME((uint32_t)(dist_meters / req->walk_speed + req->walk_slack));
            rtime_t time_to = req->arrive_by ? time_from - transfer_duration
                                             : time_from + transfer_duration;
            /* Avoid reserved values including UNREACHED */
            if (time_to > RTIME_THREE_DAYS) continue; 
            /* Catch wrapping/overflow due to limited range of rtime_t (happens normally on overnight routing but should be avoided rather than caught) */
            if (req->arrive_by ? time_to > time_from : time_to < time_from) continue;
            I printf ("    target %d %s (%s) \n", stop_index_to, timetext(router->best_time[stop_index_to]), tdata_stop_name_for_index(router->tdata, stop_index_to));
            I printf ("    transfer time   %s\n", timetext(transfer_duration));
            I printf ("    transfer result %s\n", timetext(time_to));
            router_state_t *state_to = states + stop_index_to;
            // TODO verify state_to->walk_time versus router->best_time[stop_index_to]
            if (router->best_time[stop_index_to] == UNREACHED || (req->arrive_by ? time_to > router->best_time[stop_index_to]
                                                                                 : time_to < router->best_time[stop_index_to])) {
                I printf ("      setting %d to %s\n", stop_index_to, timetext(time_to));
                state_to->walk_time = time_to; 
                state_to->walk_from = stop_index_from;
                router->best_time[stop_index_to] = time_to;
                flag_routes_for_stop (router, req, stop_index_to);
                unflag_banned_routes (router, req);
            }
        }
    }
    /* Done with all transfers, reset stop-reached bits for the next round */
    bitset_reset (router->updated_stops);
    /* 
      Check invariant: 
      Every stop reached in this round should have a best time equal to its walk time,
      and a walk arrival time <= its ride arrival time.
    */
}

static void dump_results(router_t *router) {
    router_state_t (*states)[router->tdata->n_stops] = (void*) router->states;
    // char id_fmt[10];
    // sprintf(id_fmt, "%%%ds", router->tdata->stop_id_width);
    char *id_fmt = "%30.30s";
    printf("\nRouter states:\n");
    printf(id_fmt, "Stop name");
    printf(" [sindex]");
    for (uint32_t r = 0; r < RRRR_MAX_ROUNDS; ++r){
        printf("  round %d   walk %d", r, r);
    }
    printf("\n");
    for (uint32_t stop = 0; stop < router->tdata->n_stops; ++stop) {
        bool set = false;
        for (uint32_t round = 0; round < RRRR_MAX_ROUNDS; ++round) {
            if (states[round][stop].walk_time != UNREACHED) {
                set = true;
                break;
            } 
        }
        if ( ! set) continue;
        char *stop_id = tdata_stop_name_for_index (router->tdata, stop);
        printf(id_fmt, stop_id);
        printf(" [%6d]", stop);
        for (uint32_t round = 0; round < RRRR_MAX_ROUNDS; ++round) {
            printf(" %8s", timetext(states[round][stop].time));
            printf(" %8s", timetext(states[round][stop].walk_time));
        }
        printf("\n");
    }
    printf("\n");
}

// WARNING we are not currently storing trip IDs so this will segfault
void dump_trips(router_t *router) {
    uint32_t n_routes = router->tdata->n_routes;
    for (uint32_t ridx = 0; ridx < n_routes; ++ridx) {
        route_t route = router->tdata->routes[ridx];
        char (*trip_ids)[router->tdata->trip_id_width] = (void*)
            tdata_trip_ids_for_route(router->tdata, ridx);
        calendar_t *trip_masks = tdata_trip_masks_for_route(router->tdata, ridx);
        printf ("route %d (of %d), n trips %d, n stops %d\n", ridx, n_routes, route.n_trips, route.n_stops);
        for (uint32_t tidx = 0; tidx < route.n_trips; ++tidx) {
            printf ("trip index %d trip_id %s mask ", tidx, trip_ids[tidx]);
            printBits (4, & (trip_masks[tidx]));
            printf ("\n");
        }
    }
    exit(0);
}

/* Find a suitable trip to board at the given time and stop.
   Returns the trip index within the route. */
uint32_t find_departure(route_t *route, stoptime_t (*stop_times)[route->n_stops]) {
    return NONE;
}

static void day_mask_dump (calendar_t mask) {
    printf ("day mask: ");
    printBits (4, &mask);
    printf ("bits set: ");
    for (int i = 0; i < 32; ++i) if (mask & (1 << i)) printf ("%d ", i);
    printf ("\n");
}

static void service_day_dump (serviceday_t *serviceday) {
    printf ("service day\nmidnight: %s \n", timetext(serviceday->midnight));
    day_mask_dump (serviceday->mask);
    printf ("real-time: %s \n\n", serviceday->apply_realtime ? "YES" : "NO");
}

static inline rtime_t 
tdata_depart (tdata_t* td, trip_t *trip, uint32_t route_stop) {
    return trip->begin_time + td->stop_times[trip->stop_times_offset + route_stop].departure;
}

static inline rtime_t 
tdata_arrive (tdata_t* td, trip_t *trip, uint32_t route_stop) {
    return trip->begin_time + td->stop_times[trip->stop_times_offset + route_stop].arrival;
}

/* Get the departure or arrival time of the given trip on the given service day, applying realtime data as needed. */
static inline rtime_t 
tdata_stoptime (tdata_t* tdata, trip_t *trip, uint32_t route_stop, bool arrive, serviceday_t *serviceday) {
    rtime_t time, time_adjusted;
    if (arrive) time = tdata_arrive(tdata, trip, route_stop);
    else           time = tdata_depart(tdata, trip, route_stop);
    time_adjusted = time + serviceday->midnight;
    /*
    printf ("boarding at stop %d, time is: %s \n", route_stop, timetext (time));
    printf ("   after adjusting: %s \n", timetext (time_adjusted));
    printf ("   midnight: %d \n", serviceday->midnight);
    printf ("   delay (4sec): %d \n", trip->realtime_delay);
    */
    /* Detect overflow (this will still not catch wrapping due to negative delays on small positive times) */
    // actually this happens naturally with times like '03:00+1day' transposed to serviceday 'tomorrow'
    if (time_adjusted < time) return UNREACHED;
    /* Apply real time delay on the relevant days. */
    if (serviceday->apply_realtime) time_adjusted += trip->realtime_delay;
    return time_adjusted;
}

bool router_route(router_t *router, router_request_t *req) {
    // router_request_dump(router, preq);
    uint32_t n_stops = router->tdata->n_stops;
    router->day_mask = req->day_mask;
    
    /* One serviceday_t for each of: yesterday, today, tomorrow (for overnight searches) */
    /* Note that yesterday's bit flag will be 0 if today is the first day of the calendar. */
    {
        // One bit for the calendar day on which realtime data should be applied (applying only on the true current calendar day)
        calendar_t realtime_mask = 1 << ((time(NULL) - router->tdata->calendar_start_time) / SEC_IN_ONE_DAY);
        serviceday_t yesterday;
        yesterday.midnight = 0;
        yesterday.mask = router->day_mask >> 1;
        yesterday.apply_realtime = yesterday.mask & realtime_mask;
        serviceday_t today;
        today.midnight = RTIME_ONE_DAY;
        today.mask = router->day_mask;
        today.apply_realtime = today.mask & realtime_mask;
        serviceday_t tomorrow;
        tomorrow.midnight = RTIME_TWO_DAYS;
        tomorrow.mask = router->day_mask << 1;
        tomorrow.apply_realtime = tomorrow.mask & realtime_mask;
        /* Iterate backward over days for arrive-by searches. */
        if (req->arrive_by) {
            router->servicedays[0] = tomorrow;
            router->servicedays[1] = today;
            router->servicedays[2] = yesterday;        
        } else {
            router->servicedays[0] = yesterday;
            router->servicedays[1] = today;
            router->servicedays[2] = tomorrow;        
        }        
        /* set day_mask to catch all service days (0, 1, 2) */
        router->day_mask = yesterday.mask | today.mask | tomorrow.mask;
    }
    // for (int i = 0; i < 3; ++i) service_day_dump (&routers->servicedays[i]);
    // day_mask_dump (router->day_mask); 

    I router_request_dump(router, req);
    T printf("\norigin_time %s \n", timetext(req->time));
    T tdata_dump(router->tdata);
    
    I printf("Initializing router state \n");
    // Router state is a C99 dynamically dimensioned array of size [RRRR_MAX_ROUNDS][n_stops]
    router_state_t (*states)[n_stops] = (router_state_t(*)[]) router->states; 
    for (uint32_t round = 0; round < RRRR_MAX_ROUNDS; ++round) {
        for (uint32_t stop = 0; stop < n_stops; ++stop) {
            // We use the time fields to record when stops have been reached. 
            // When times are UNREACHED the other fields in the same state should never be read.
            states[round][stop].time = UNREACHED;
            states[round][stop].walk_time = UNREACHED;
            /*
            states[round][stop].back_stop = NONE;
            states[round][stop].back_route = NONE;
            states[round][stop].back_trip = NONE;
            states[round][stop].board_time = UNREACHED;
            states[round][stop].back_trip_id = NULL; 
            */
        }
    }
    for (uint32_t s = 0; s < n_stops; ++s) router->best_time[s] = UNREACHED;

    /* Stop indexes where the search process begins and ends, independent of arrive_by */
    if (req->arrive_by) {
        router->origin = req->to;
        router->target = req->from;
    } else {
        router->origin = req->from;
        router->target = req->to;
    }
    
    if (req->start_trip_route != NONE && req->start_trip_trip != NONE) {
        /* We are starting on board a trip, not at a station. */
        /* On-board departure only makes sense for depart-after requests. */
        if (req->arrive_by) {
            fprintf (stderr, "An arrive-by search does not make any sense if you are starting on-board.\n");
            return false;
        }
        /* 
          We cannot expand the start trip into the temporary round (1) during initialization because we may be able to 
          reach the destination on that starting trip.
          We discover the previous stop and flag only the selected route for exploration in round 0. This would 
          interfere with search reversal, but reversal is meaningless/useless in on-board depart trips anyway.
        */
        route_t  route = router->tdata->routes[req->start_trip_route];
        trip_t   *trip = tdata_trips_for_route (router->tdata, req->start_trip_route) + req->start_trip_trip;
        uint32_t *route_stops   = tdata_stops_for_route(router->tdata, req->start_trip_route);
        uint32_t prev_stop      = NONE;
        rtime_t  prev_stop_time = UNREACHED;
        // add tdata function to return next stop and stoptime given route, trip, and time
        for (int route_stop = 0; route_stop < route.n_stops; ++route_stop) {
            uint32_t stop = route_stops[route_stop];
            rtime_t time = tdata_stoptime (router->tdata, trip, route_stop, false, &(router->servicedays[1]));
            /* Find stop immediately after the given time on the given trip. */
            if (req->arrive_by ? time > req->time : time < req->time) {
                if (prev_stop_time == UNREACHED || (req->arrive_by ? time < prev_stop_time : time > prev_stop_time)) {
                    prev_stop = stop;
                    prev_stop_time = time;
                }
            }
        }
        if (prev_stop != NONE) {
            /* rewrite the request to begin at the previous stop on the starting trip */
            char *prev_stop_id = tdata_stop_name_for_index(router->tdata, prev_stop);  //TODO Andrew shouldn't this be stop_id?
            // printf ("Based on start trip and time, chose previous stop %s [%d] at %s\n", prev_stop_id, prev_stop, timetext(prev_stop_time));
            req->from = ONBOARD;
            /* Initialize origin state */
            router->origin = prev_stop; // only origin is used from here on in routing
            router->best_time[router->origin]   = prev_stop_time;
            states[1][router->origin].time      = prev_stop_time;
            states[1][router->origin].walk_time = prev_stop_time; 
            /* When starting on board, only flag one route and do not apply transfers, only a single walk. */
            bitset_reset (router->updated_stops);
            bitset_reset (router->updated_routes);
            bitset_set (router->updated_routes, req->start_trip_route);
        }
    }
    
    /* Initialize origin state if not beginning the search on board. */
    if (req->from != ONBOARD) {
        /* We will use round 1 to hold the initial state for round 0. Round 1 must then be re-initialized before use. */
        router->best_time[router->origin] = req->time;
        states[1][router->origin].time   = req->time;
        // the rest of these should be unnecessary
        states[1][router->origin].back_stop  = NONE;
        states[1][router->origin].back_route = NONE;
        states[1][router->origin].back_trip  = NONE;
        states[1][router->origin].board_time = UNREACHED;
        /* Hack to communicate the origin time to itinerary renderer. It would be better to just include rtime_t in request structs. */
        // TODO eliminate this now that we have rtimes in requests
        states[0][router->origin].time = req->time;
        bitset_reset(router->updated_stops);
        // This is inefficient, as it depends on iterating over a bitset with only one bit true.
        bitset_set(router->updated_stops, router->origin);
        // Remove the banned stops from the bitset (do we really want to do this here? this could only remove the origin stop.)
        unflag_banned_stops(router, req);
        // Apply transfers to initial state, which also initializes the updated routes bitset.
        apply_transfers(router, req, 1);
        // dump_results(router);
    }    
    
    /* apply upper bounds (speeds up second and third reversed searches) */
    uint32_t n_rounds = req->max_transfers + 1;
    if (n_rounds > RRRR_MAX_ROUNDS)
        n_rounds = RRRR_MAX_ROUNDS;

    // Iterate over rounds. In round N, we have made N transfers.
    for (uint8_t round = 0; round < n_rounds; ++round) {  // < n_rounds to apply upper bound on transfers...
        router_round(router, req, round);
    } // end for (round)
    return true;
}

void router_round(router_t *router, router_request_t *req, uint8_t round) {
    // TODO restrict pointers?
    uint32_t n_stops = router->tdata->n_stops;
    router_state_t (*states)[n_stops] = (router_state_t(*)[]) router->states;
    uint8_t last_round = (round == 0) ? 1 : round - 1;

    I printf("round %d\n", round);
    // Iterate over all routes which contain a stop that was updated in the last round.
    for (uint32_t route_idx  = bitset_next_set_bit (router->updated_routes, 0); 
                    route_idx != BITSET_NONE;
                    route_idx  = bitset_next_set_bit (router->updated_routes, route_idx + 1)) {
        route_t route = router->tdata->routes[route_idx]; // really, 'trip' should be a trip_t to follow this same convention, and trip_idx should be its index
        bool route_overlap = route.min_time < route.max_time - RTIME_ONE_DAY;
        /*
        if (route_overlap) printf ("min time %d max time %d overlap %d \n", route.min_time, route.max_time, route_overlap);
        printf ("route %d has min_time %d and max_time %d. \n", route_idx, route.min_time, route.max_time);
        printf ("  actual first time: %d \n", tdata_depart(router->tdata, route_idx, 0, 0));
        printf ("  actual last time:  %d \n", tdata_arrive(router->tdata, route_idx, route.n_trips - 1, route.n_stops - 1));
        */
        I printf("  route %d: %s;%s\n", route_idx, tdata_shortname_for_route(router->tdata, route_idx),tdata_headsign_for_route(router->tdata, route_idx));
        T tdata_dump_route(router->tdata, route_idx, NONE);
        // For each stop in this route, its global stop index.
        uint32_t *route_stops = tdata_stops_for_route(router->tdata, route_idx);
        uint8_t  *route_stop_attributes = tdata_stop_attributes_for_route(router->tdata, route_idx);
        trip_t   *route_trips = tdata_trips_for_route(router->tdata, route_idx); // TODO use to avoid calculating at every stop
        uint8_t  *route_trip_attributes = tdata_trip_attributes_for_route(router->tdata, route_idx);
        calendar_t *trip_masks  = tdata_trip_masks_for_route(router->tdata, route_idx); 
        uint32_t  trip = NONE; // trip index within the route. NONE means not yet boarded.
        uint32_t  board_stop;  // stop index where that trip was boarded
        rtime_t   board_time;  // time when that trip was boarded
        serviceday_t *board_serviceday;  // Service day on which that trip was boarded
        /*
            Iterate over stop indexes within the route. Each one corresponds to a global stop index.
            Note that the stop times array should be accessed with [trip][route_stop] not [trip][stop].
            The iteration variable is signed to allow ending the iteration at the beginning of the route.
        */
        for (int route_stop = req->arrive_by ? route.n_stops - 1 : 0;
                                req->arrive_by ? route_stop >= 0 : route_stop < route.n_stops; 
                                req->arrive_by ? --route_stop : ++route_stop ) {
            uint32_t stop = route_stops[route_stop];
            I printf("    stop %2d [%d] %s %s\n", route_stop, stop,
                timetext(router->best_time[stop]), tdata_stop_name_for_index (router->tdata, stop));

            /*
                If a stop in in banned_stop_hard, we do not want to transit through this station
                we reset the current trip to NONE and skip the currect stop.
                This effectively splits the route in two, and forces a re-board afterwards.
            */
            for (uint32_t bsh = 0; bsh < req->n_banned_stops_hard; bsh++) {
                if (stop == req->banned_stop_hard) {
                    trip = NONE;
                    continue;
                }
            }

            /* 
                If we are not already on a trip, or if we might be able to board a better trip on 
                this route at this location, indicate that we want to search for a trip.
            */
            bool attempt_board = false;
            rtime_t prev_time = states[last_round][stop].walk_time;
            if (prev_time != UNREACHED) { // Only board at placed that have been reached.
                if (trip == NONE || req->via == stop) {
                    attempt_board = true;
                } else if (trip != NONE && req->via != NONE && req->via == board_stop) {
                    attempt_board = false;
                } else {
                    // removed xfer slack for simplicity
                    // is this repetitively triggering re-boarding searches along a single route?
                    rtime_t trip_time = tdata_stoptime (router->tdata, &(route_trips[trip]), route_stop, req->arrive_by, board_serviceday);
                    if (trip_time == UNREACHED) attempt_board = false;
                    else if (req->arrive_by ? prev_time > trip_time
                                            : prev_time < trip_time) {
                        attempt_board = true;
                        I printf ("    [reboarding here] trip = %s\n", timetext(trip_time));
                    }
                }
            }

            if (!(route_stop_attributes[route_stop] & rsa_boarding)) //Boarding not allowed
                if (req->arrive_by ? trip != NONE : attempt_board) //and we're attempting to board
                    continue; //Boarding not allowed and attemping to board
            if (!(route_stop_attributes[route_stop] & rsa_alighting)) //Alighting not allowed
                if (req->arrive_by ? attempt_board : trip != NONE) //and we're seeking to alight
                    continue; //Alighting not allowed and attemping to alight

            /* If we have not yet boarded a trip on this route, see if we can board one.
                Also handle the case where we hit a stop with an existing better arrival time. */
            // TODO: check if this is the last stop -- no point boarding there or marking routes
            if (attempt_board) {
                I printf ("    attempting boarding at stop %d\n", stop);
                T tdata_dump_route(router->tdata, route_idx, NONE);
                /* Scan all trips to find the soonest trip that can be boarded, if any.
                    Real-time updates can ruin FIFO ordering of trips within routes.
                    Scanning through the whole list of trips reduces speed by ~20 percent over binary search. */
                uint32_t best_trip = NONE;
                rtime_t  best_time = req->arrive_by ? 0 : UINT16_MAX;
                serviceday_t  *best_serviceday;
                /* Search trips within days. The loop nesting could also be inverted. */
                for (serviceday_t *serviceday = router->servicedays; serviceday <= router->servicedays + 2; ++serviceday) {
                    /* Check that this route still has any trips running on this day. */
                    if (req->arrive_by ? prev_time < serviceday->midnight + route.min_time
                                        : prev_time > serviceday->midnight + route.max_time) continue;
                    /* Check whether there's any chance of improvement by scanning additional days. */ 
                    /* Note that day list is reversed for arrive-by searches. */
                    if (best_trip != NONE && ! route_overlap) break;
                    for (uint32_t this_trip = 0; this_trip < route.n_trips; ++this_trip) {
                        // D printBits(4, & (trip_masks[this_trip]));
                        // D printBits(4, & (serviceday->mask));
                        // D printf("\n");
                        /* skip this trip if it is banned */
                        for (uint32_t bt = 0; bt < req->n_banned_trips; bt++) if (route_idx == req->banned_trip_route && this_trip == req->banned_trip_offset) continue;
                        /* skip this trip if it is not running on the current service day */
                        if ( ! (serviceday->mask & trip_masks[this_trip])) continue;
                        /* skip this trip if it doesn't have all our required attributes */
                        if ( ! ((req->trip_attributes & route_trip_attributes[this_trip]) == req->trip_attributes)) continue;
                        /* skip this trip if the realtime delay equals CANCELED */
                        if ( route_trips[this_trip].realtime_delay == CANCELED) continue;
                        
                        /* consider the arrival or departure time on the current service day */ 
                        rtime_t time = tdata_stoptime (router->tdata, &(route_trips[this_trip]), route_stop, req->arrive_by, serviceday);
                        // T printf("    board option %d at %s \n", this_trip, ...
                        if (time == UNREACHED) continue; // rtime overflow due to long overnight trips on day 2
                        /* Mark trip for boarding if it improves on the last round's post-walk time at this stop.
                            Note: we should /not/ be comparing to the current best known time at this stop, because
                            it may have been updated in this round by another trip (in the pre-walk transit phase). */
                        if (req->arrive_by ? time <= prev_time && time > best_time
                                            : time >= prev_time && time < best_time) {
                            best_trip = this_trip;
                            best_time = time;
                            best_serviceday = serviceday;
                        }
                    } // end for (trips within this route)
                } // end for (service days: yesterday, today, tomorrow)
                if (best_trip != NONE) {
                    I printf("    boarding trip %d at %s \n", best_trip, timetext(best_time));
                    if ((req->arrive_by ? best_time > req->time : best_time < req->time) && req->from != ONBOARD) {
                        printf("ERROR: boarded before start time, trip %d stop %d \n", best_trip, stop);
                    } else {
                        // use a router_state struct for all this?
                        board_time = best_time;
                        board_stop = stop;
                        board_serviceday = best_serviceday;
                        trip = best_trip;
                    }
                } else {
                    T printf("    no suitable trip to board.\n");
                }
                continue; // to the next stop in the route
            } else if (trip != NONE) { // We have already boarded a trip along this route.
                rtime_t time = tdata_stoptime (router->tdata, &(route_trips[trip]), route_stop, !req->arrive_by, board_serviceday);
                if (time == UNREACHED) continue; // overflow due to long overnight trips on day 2
                T printf("    on board trip %d considering time %s \n", trip, timetext(time)); 
                // Target pruning, sec. 3.1 of RAPTOR paper.
                if ((router->best_time[router->target] != UNREACHED) && 
                    (req->arrive_by ? time < router->best_time[router->target] 
                                    : time > router->best_time[router->target])) { 
                    T printf("    (target pruning)\n");
                    // We cannot break out of this route entirely, because re-boarding may occur at a later stop.
                    continue;
                }
                if ((req->time_cutoff != UNREACHED) && 
                    (req->arrive_by ? time < req->time_cutoff 
                                    : time > req->time_cutoff)) {
                    continue;
                }
                // Do we need best_time at all? yes, because the best time may not have been found in the previous round.
                bool improved = (router->best_time[stop] == UNREACHED) || 
                                (req->arrive_by ? time > router->best_time[stop] 
                                                : time < router->best_time[stop]);
                if (!improved) {
                    I printf("    (no improvement)\n");
                    continue; // the current trip does not improve on the best time at this stop
                }
                if (time > RTIME_THREE_DAYS) {
                    /* Reserve all time past three days for special values like UNREACHED. */
                } else if (req->arrive_by ? time > req->time : time < req->time) {
                    /* Wrapping/overflow. This happens due to overnight trips on day 2. Prune them. */
                    // printf("ERROR: setting state to time before start time. route %d trip %d stop %d \n", route_idx, trip, stop);
                } else { // TODO should alighting handled here? if ((route_stop_attributes[route_stop] & rsa_alighting) == rsa_alighting)
                    I printf("    setting stop to %s \n", timetext(time)); 
                    router->best_time[stop] = time;
                    states[round][stop].time = time;
                    states[round][stop].back_route = route_idx; 
                    states[round][stop].back_trip  = trip; 
                    states[round][stop].back_stop  = board_stop;
                    states[round][stop].board_time = board_time;
                    if (req->arrive_by) {
                        if (board_time < time) printf ("board time non-decreasing\n");
                    } else {
                        if (board_time > time) printf ("board time non-increasing\n");
                    }
                    bitset_set(router->updated_stops, stop);   // mark stop for next round.
                }
            }
        } // end for (stop)
    } // end for (route)
    // Remove the banned stops from the bitset, so no transfers will happen there.
    unflag_banned_stops(router, req);
    /* Also updates the list of routes for next round based on stops that were touched in this round. */
    apply_transfers(router, req, round);
    // exit(0);
    /* Initialize the stops in round 1 that were used as starting points for round 0. */
    if (round == 0) initialize_transfers (router, 1, router->origin);
    // dump_results(router); // DEBUG

}

/* Reverse the times and stops in a leg. Used for creating arrive-by itineraries. */
static inline void leg_swap (struct leg *leg) {
    struct leg temp = *leg;
    leg->s0 = temp.s1;
    leg->s1 = temp.s0;
    leg->t0 = temp.t1;
    leg->t1 = temp.t0;
}

/* Checks charateristics that should be the same for all trip plans produced by this router:
   All stops should chain, all times should be increasing, all waits should be at the ends of walk legs, etc.
   Returns true if any of the checks fail, false if no problems are detected. */
static bool check_plan_invariants (struct plan *plan) {
    bool fail = false;
    struct itinerary *prev_itin = NULL;
    rtime_t prev_target_time = UNREACHED;
    /* Loop over all itineraries in this plan. */
    for (uint32_t i = 0; i < plan->n_itineraries; ++i) {
        struct itinerary *itin = plan->itineraries + i;
        if (itin->n_legs < 1) {
            fprintf(stderr, "itinerary contains no legs.\n");
            fail = true;
        } else {
            /* Itinarary has at least one leg. Grab its first and last leg. */
            struct leg *leg0 = itin->legs;
            struct leg *legN = itin->legs + (itin->n_legs - 1);
            /* Itineraries should be Pareto-optimal. Increase in number of rides implies improving arrival time. */
            rtime_t target_time = plan->req.arrive_by ? leg0->t0 : legN->t1;
            if (i > 0) {
                if (itin->n_legs <= prev_itin->n_legs) {
                    fprintf(stderr, "itineraries do not have strictly increasing numbers of legs: %d, %d.\n", prev_itin->n_legs, itin->n_legs);
                    fail = true;
                }
                if (plan->req.arrive_by ? target_time <= prev_target_time : target_time >= prev_target_time) {
                    fprintf(stderr, "itineraries do not have strictly improving target times: %d, %d.\n", prev_target_time, target_time);
                    fail = true;
                }
            }
            prev_target_time = target_time;
            prev_itin = itin;
            /* Check that itinerary does indeed connect the places in the request. */
            if (leg0->s0 != plan->req.from) {
                fprintf(stderr, "itinerary does not begin at from location.\n");
                fail = true;
            }
            if (legN->s1 != plan->req.to) {
                fprintf(stderr, "itinerary does not end at to location.\n");
                fail = true;
            }
            /* Check that the itinerary respects the depart after or arrive-by criterion */
            /* finish when rtimes are in requests
            if (plan->req.arrive_by) {
                if (itin->legs[itin->n_legs - 1].s1 > plan->req.time)...
            } else {
                if (itin->legs[0].s0 < plan->req.time)...
            }
            */
            /* All itineraries are composed of ride-walk pairs, prefixed by a single walk leg. */
            if (itin->n_legs % 2 != 1) {
                fprintf(stderr, "itinerary has an inexplicable (even) number of legs: %d\n", itin->n_legs);
                fail = true;
            }
        }
        /* Check per-leg invariants within each itinerary. */
        struct leg *prev_leg = NULL;
        for (uint32_t l = 0; l < itin->n_legs; ++l) {
            struct leg *leg = itin->legs + l;
            if (l % 2 == 0) {
                if (leg->route != WALK) fprintf(stderr, "even numbered leg %d has route %d not WALK.\n", l, leg->route);
                fail = true;
            } else {
                if (leg->route == WALK) fprintf(stderr, "odd numbered leg %d has route WALK.\n", l);
                fail = true;
            }
            if (leg->t1 < leg->t0) {
                fprintf(stderr, "non-increasing times within leg %d: %d, %d\n", l, leg->t0, leg->t1);
                fail = true;
            }
            if (l > 0) {
                if (leg->s0 != prev_leg->s1) {
                    fprintf(stderr, "legs do not chain: leg %d begins with stop %d, previous leg ends with stop %d.\n", l, leg->s0, prev_leg->s1);
                    fail = true;
                }
                if (leg->route == WALK && leg->t0 != prev_leg->t1) {
                    /* This will fail unless reversal is being performed */
                    // fprintf(stderr, "walk leg does not immediately follow ride: leg %d begins at time %d, previous leg ends at time %d.\n", l, leg->t0, prev_leg->t1);
                    // fail = true;
                }
                if (leg->t0 < prev_leg->t1) {
                    fprintf(stderr, "itin %d: non-increasing times between legs %d and %d: %d, %d\n", i, l - 1, l, prev_leg->t1, leg->t0);
                    fail = true;
                }
            }
            prev_leg = leg;
        } /* End for (legs) */
    } /* End for (itineraries) */
    return fail;
}

void router_result_to_plan (struct plan *plan, router_t *router, router_request_t *req) {
    uint32_t n_stops = router->tdata->n_stops;
    /* Router states are a 2D array of stride n_stops */
    router_state_t (*states)[n_stops] = (router_state_t(*)[]) router->states;
    plan->n_itineraries = 0;
    plan->req = *req; // copy the request into the plan for use in rendering
    struct itinerary *itin = plan->itineraries;
    /* Loop over the rounds to get ending states of itineraries using different numbers of vehicles */
    for (int n_xfers = 0; n_xfers < RRRR_MAX_ROUNDS; ++n_xfers) {
        /* Work backward from the target to the origin */
        uint32_t stop = (req->arrive_by ? req->from : req->to);
        /* skip rounds that were not reached */
        if (states[n_xfers][stop].walk_time == UNREACHED) continue;
        itin->n_rides = n_xfers + 1;
        itin->n_legs = itin->n_rides * 2 + 1; // always same number of legs for same number of transfers
        struct leg *l = itin->legs; // the slot in which record a leg, reversing them for forward trips
        if ( ! req->arrive_by) l += itin->n_legs - 1;
        /* Follow the chain of states backward */
        for (int round = n_xfers; round >= 0; --round) {
            if (stop > router->tdata->n_stops) { 
                printf ("ERROR: stopid %d out of range.\n", stop);
                break;
            }
                        
            /* Walk phase */
            router_state_t *walk = &(states[round][stop]);
            if (walk->walk_time == UNREACHED) {
                printf ("ERROR: stop %d was unreached by walking.\n", stop);
                break;
            } 
            uint32_t walk_stop = stop;
            stop = walk->walk_from;  /* follow the chain of states backward */

            /* Ride phase */
            router_state_t *ride = &(states[round][stop]);
            if (ride->time == UNREACHED) {
                printf ("ERROR: stop %d was unreached by riding.\n", stop);
                break;
            } 
            uint32_t ride_stop = stop;
            stop = ride->back_stop;  /* follow the chain of states backward */                                    
            
            /* Walk phase */
            l->s0 = walk->walk_from;
            l->s1 = walk_stop;
            l->t0 = ride->time; /* Rendering the walk requires already having the ride arrival time */
            l->t1 = walk->walk_time;
            l->route = WALK;
            l->trip  = WALK;
            if (req->arrive_by) leg_swap (l);
            l += (req->arrive_by ? 1 : -1); /* next leg */

            /* Ride phase */
            l->s0 = ride->back_stop;
            l->s1 = ride_stop;
            l->t0 = ride->board_time;
            l->t1 = ride->time;
            l->route = ride->back_route;
            l->trip  = ride->back_trip;
            if (req->arrive_by) leg_swap (l);
            l += (req->arrive_by ? 1 : -1);   /* next leg */
            
        }        
        if (req->start_trip_trip != NONE) {
            /* Results starting on board do not have an initial walk leg. */        
            l->s0 = l->s1 = ONBOARD;
            l->t0 = l->t1 = req->time;
            l->route = l->trip = WALK;
            l += 1; // move back to first transit leg
            l->s0 = ONBOARD;
            l->t0 = req->time;
        } else {
            /* The initial walk leg leading out of the search origin. This is inferred, not stored explicitly. */ 
            router_state_t *final_walk = &(states[0][stop]);
            uint32_t origin_stop = (req->arrive_by ? req->to : req->from);
            l->s0 = origin_stop;
            l->s1 = stop;
            /* It would also be possible to work from s1 to s0 and compress out the wait time. */
            l->t0 = states[0][origin_stop].time;
            rtime_t duration = transfer_duration (router->tdata, req, l->s0, l->s1);
            l->t1 = l->t0 + (req->arrive_by ? -duration : +duration);
            l->route = WALK;
            l->trip  = WALK;
            if (req->arrive_by) leg_swap (l);
        }
        /* Move to the next itinerary in the plan. */
        plan->n_itineraries += 1;
        itin += 1;
    }
    check_plan_invariants (plan);
}

static inline char *plan_render_itinerary (struct itinerary *itin, tdata_t *tdata, char *b, char *b_end) {
    b += sprintf (b, "\nITIN %d rides \n", itin->n_rides);

    /* Render the legs of this itinerary, which are in chronological order */
    for (struct leg *leg = itin->legs; leg < itin->legs + itin->n_legs; ++leg) {
        char ct0[16];
        char ct1[16];
        btimetext(leg->t0, ct0);
        btimetext(leg->t1, ct1);
        char *s0_id = tdata_stop_name_for_index(tdata, leg->s0);
        char *s1_id = tdata_stop_name_for_index(tdata, leg->s1);
        char *agency_name = (leg->route == WALK) ? "" : tdata_agency_name_for_route (tdata, leg->route);
        char *short_name = (leg->route == WALK) ? "walk" : tdata_shortname_for_route (tdata, leg->route);
        char *headsign = (leg->route == WALK) ? "walk" : tdata_headsign_for_route (tdata, leg->route);
        char *productcategory = (leg->route == WALK) ? "" : tdata_productcategory_for_route (tdata, leg->route);
        float delay_min = (leg->route == WALK) ? 0 : tdata_delay_min (tdata, leg->route, leg->trip);
        
        char *leg_mode = NULL;
        if (leg->route == WALK) {
            /* Skip uninformative legs that just tell you to stay in the same place. if (leg->s0 == leg->s1) continue; */
            if (leg->s0 == ONBOARD) continue;
            if (leg->s0 == leg->s1) leg_mode = "WAIT";
            else leg_mode = "WALK";
        } else
        if ((tdata->routes[leg->route].attributes & m_tram)      == m_tram)      leg_mode = "TRAM";      else
        if ((tdata->routes[leg->route].attributes & m_subway)    == m_subway)    leg_mode = "SUBWAY";    else
        if ((tdata->routes[leg->route].attributes & m_rail)      == m_rail)      leg_mode = "RAIL";      else
        if ((tdata->routes[leg->route].attributes & m_bus)       == m_bus)       leg_mode = "BUS";       else
        if ((tdata->routes[leg->route].attributes & m_ferry)     == m_ferry)     leg_mode = "FERRY";     else
        if ((tdata->routes[leg->route].attributes & m_cablecar)  == m_cablecar)  leg_mode = "CABLE_CAR"; else
        if ((tdata->routes[leg->route].attributes & m_gondola)   == m_gondola)   leg_mode = "GONDOLA";   else
        if ((tdata->routes[leg->route].attributes & m_funicular) == m_funicular) leg_mode = "FUNICULAR"; else
        leg_mode = "INVALID";

        char *alert_msg = NULL;
        if (leg->route != WALK && tdata->alerts) {
            for (size_t e = 0; e < tdata->alerts->n_entity; ++e) {
                TransitRealtime__FeedEntity *entity = tdata->alerts->entity[e];
                if (entity == NULL) break;
                TransitRealtime__Alert *alert = entity->alert;
                if (alert == NULL) break;
                for (size_t ie = 0; ie < alert->n_informed_entity; ++ie) {
                    TransitRealtime__EntitySelector *informed_entity = alert->informed_entity[ie];
                    // TransitRealtime__TripDescriptor *trip = informed_entity->trip;
                    
                    if ( ( (!informed_entity->route_id) || (memcmp(informed_entity->route_id, &leg->route, sizeof(leg->route)) == 0 ) ) &&
                         ( (!informed_entity->stop_id)  || (memcmp(informed_entity->stop_id, &leg->s0, sizeof(leg->s0)) == 0 ) ) &&
                         ( (!informed_entity->trip)     || (!informed_entity->trip->trip_id) || (memcmp(informed_entity->trip->trip_id, &leg->trip, sizeof(leg->trip)) == 0 ) ) 
                         // TODO: need to have rtime_to_date  for informed_entity->trip->start_date
                         // TODO: need to have rtime_to_epoch for informed_entity->active_period
                       ) {
                        alert_msg = alert->header_text->translation[0]->text;
                    }

                    if (alert_msg) break;
                }

                if (alert_msg) break;
            }
        }

        b += sprintf (b, "%s %5d %3d %5d %5d %s %s %+3.1f ;%s;%s;%s;%s;%s;%s;%s\n",
            leg_mode, leg->route, leg->trip, leg->s0, leg->s1, ct0, ct1, delay_min,agency_name, short_name, headsign, productcategory, s0_id, s1_id,
            (alert_msg ? alert_msg : ""));

        /* EXAMPLE
        polyline_for_leg (tdata, leg);
        b += sprintf (b, "%s\n", polyline_result());
        */
        
        if (b > b_end) {
            printf ("buffer overflow\n");
            exit(2);
        }
    }

    return b;
}

/* Write a plan structure out to a text buffer in tabular format. */
static inline uint32_t plan_render(struct plan *plan, tdata_t *tdata, router_request_t *req, char *buf, uint32_t buflen) {
    char *b = buf;
    char *b_end = buf + buflen;

    if ((req->optimise & o_all) == o_all) {
        /* Iterate over itineraries in this plan, which are in increasing order of number of rides */
        for (struct itinerary *itin = plan->itineraries; itin < plan->itineraries + plan->n_itineraries; ++itin) {
            b = plan_render_itinerary (itin, tdata, b, b_end);
        }
    } else if (plan->n_itineraries > 0) {
        if ((req->optimise & o_transfers) == o_transfers) { 
            /* only render the first itinerary, which has the least transfers */
            b = plan_render_itinerary (plan->itineraries, tdata, b, b_end);
        }
        if ((req->optimise & o_shortest) == o_shortest) {
            /* only render the last itinerary, which has the most rides and is the shortest in time */
            b = plan_render_itinerary (&plan->itineraries[plan->n_itineraries - 1], tdata, b, b_end);
        }
    }
    *b = '\0';
    return b - buf;
}

/*
  After routing, call to convert the router state into a readable list of itinerary legs.
  Returns the number of bytes written to the buffer.
*/
uint32_t router_result_dump(router_t *router, router_request_t *req, char *buf, uint32_t buflen) {
    struct plan plan;
    router_result_to_plan (&plan, router, req);
    // plan_render_json (&plan, router->tdata, req);
    return plan_render (&plan, router->tdata, req, buf, buflen);
}

uint32_t rrrrandom(uint32_t limit) {
    return (uint32_t) (limit * (random() / (RAND_MAX + 1.0)));
}

void router_request_initialize(router_request_t *req) {
    req->walk_speed = 1.5; // m/sec
    req->walk_slack = RRRR_WALK_SLACK_SEC; // sec
    req->from = req->to = NONE;
    req->from = req->to = req->via = NONE;
    req->time = UNREACHED;
    req->time_cutoff = UNREACHED;
    req->walk_speed = 1.5; // m/sec
    req->arrive_by = true;
    req->max_transfers = RRRR_MAX_ROUNDS - 1;
    req->mode = m_all;
    req->trip_attributes = ta_none;
    req->optimise = o_all;
    req->n_banned_routes = 0;
    req->n_banned_stops = 0;
    req->n_banned_trips = 0;
    req->n_banned_stops_hard = 0;
    req->banned_route = NONE;
    req->banned_stop = NONE;
    req->banned_trip_route = NONE;
    req->banned_trip_offset = NONE;
    req->banned_stop_hard = NONE;
    req->start_trip_route = NONE;
    req->start_trip_trip  = NONE;
    req->intermediatestops = false;
}

/* Initializes the router request then fills in its time and datemask fields from the given epoch time. */
// if we set the date mask in the router itself we wouldn't need the tdata here.
void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime) {
    // char etime[32];
    // strftime(etime, 32, "%Y-%m-%d %H:%M:%S\0", localtime(&epochtime));
    // printf ("epoch time: %s [%ld]\n", etime, epochtime);
    // router_request_initialize (req);
    struct tm origin_tm;
    req->time = epoch_to_rtime (epochtime, &origin_tm);
    // TODO not DST-proof, use noons
    uint32_t cal_day = (mktime(&origin_tm) - tdata->calendar_start_time) / SEC_IN_ONE_DAY;
    if (cal_day > 31 ) {
        /* date not within validity period of the timetable file, wrap to validity range */
        cal_day %= 28; // a multiple of 7, so we always wrap to the same day of the week
        printf ("calendar day out of range. wrapping to %d, which is on the same day of the week.\n", cal_day);
        /* should set a flag in response to say that results are estimated */
    }
    req->day_mask = 1 << cal_day;    
}

void router_request_randomize (router_request_t *req, tdata_t *tdata) {
    req->walk_speed = 1.5; // m/sec
    req->walk_slack = RRRR_WALK_SLACK_SEC; // sec
    req->from = rrrrandom(tdata->n_stops);
    req->to = rrrrandom(tdata->n_stops);
    req->time = RTIME_ONE_DAY + SEC_TO_RTIME(3600 * 9 + rrrrandom(3600 * 12));
    req->via = NONE;
    req->arrive_by = rrrrandom(2); // 0 or 1
    req->time_cutoff = UNREACHED;
    req->walk_speed = 1.5; // m/sec
    req->arrive_by = rrrrandom(2); // 0 or 1
    req->max_transfers = RRRR_MAX_ROUNDS - 1;
    req->day_mask = 1 << rrrrandom(32);
    req->mode = m_all;
    req->trip_attributes = ta_none;
    req->optimise = o_all;
    req->n_banned_routes = 0;
    req->n_banned_stops = 0;
    req->n_banned_trips = 0;
    req->n_banned_stops_hard = 0;
    req->banned_route = NONE;
    req->banned_stop = NONE;
    req->banned_trip_route = NONE;
    req->banned_trip_offset = NONE;
    req->banned_stop_hard = NONE;
    req->intermediatestops = false;
}

void router_state_dump (router_state_t *state) {
    printf ("-- Router State --\n");
    printf ("walk time:    %s \n", timetext(state->walk_time));
    printf ("walk from:    %d \n", state->walk_from);
    printf ("time:         %s \n", timetext(state->time));
    printf ("board time:   %s \n", timetext(state->board_time));
    printf ("back route:   ");
    if (state->back_route == NONE) printf ("NONE\n");
    else printf ("%d\n", state->back_route);
}

/* 
   Reverse the direction of the search leaving most request parameters unchanged but applying time 
   and transfer cutoffs based on an existing result for the same request. 
   Returns a boolean value indicating whether the request was successfully reversed. 
*/
bool router_request_reverse(router_t *router, router_request_t *req) {
    router_state_t (*states)[router->tdata->n_stops] = (router_state_t(*)[]) (router->states);
    uint32_t stop = (req->arrive_by ? req->from : req->to);
    uint32_t max_transfers = req->max_transfers;
    if (max_transfers >= RRRR_MAX_ROUNDS) // range-check to keep search within states array
        max_transfers = RRRR_MAX_ROUNDS - 1;
    // find the solution with the most transfers and the earliest arrival
    uint32_t round = NONE;
    for (uint32_t r = 0; r <= max_transfers; ++r) {
        if (states[r][stop].walk_time != UNREACHED) {
            round = r;
            if (req->optimise == o_transfers) break; // use the lowest rather than highest number of transfers
        }
    }
    // In the case that no solution was found, the request will remain unchanged.
    if (round == NONE) return false;
    //printf ("State present at round %d \n", round);
    //router_state_dump (&(states[round][stop]));
    req->max_transfers = round;
    req->time_cutoff = req->time;
    req->time = states[round][stop].walk_time;
    req->arrive_by = !(req->arrive_by);
    // router_request_dump(router, req);
    // range-check the resulting request here?
    return true;
}

/*
  Check the given request against the characteristics of the router that will be used. 
  Indexes larger than array lengths for the given router, signed values less than zero, etc. 
  can and will cause segfaults and present security risks.
  
  We could also infer departure stop etc. from start trip here, "missing start point" and reversal problems.
*/
inline static bool range_check(struct router_request *req, struct router *router) {
    uint32_t n_stops = router->tdata->n_stops;
    if (req->time < 0)         return false;
    if (req->walk_speed < 0.1) return false;
    if (req->from >= n_stops)  return false;
    if (req->to >= n_stops)    return false;
    return true;
}

void router_request_dump(router_t *router, router_request_t *req) {
    char *from_stop_id = tdata_stop_name_for_index(router->tdata, req->from);
    char *to_stop_id   = tdata_stop_name_for_index(router->tdata, req->to);
    char time[32], time_cutoff[32];
    btimetext(req->time, time);
    btimetext(req->time_cutoff, time_cutoff);
    printf("-- Router Request --\n"
           "from:  %s [%d]\n"
           "to:    %s [%d]\n"
//           "date:  %s\n"
           "time:  %s [%d]\n"
           "speed: %f m/sec\n"
           "arrive-by: %s\n"
           "max xfers: %d\n"
           "max time:  %s\n"
           "mode: ",
           from_stop_id, req->from, to_stop_id, req->to, time, req->time, req->walk_speed, 
           (req->arrive_by ? "true" : "false"), req->max_transfers, time_cutoff);

    if (req->mode == m_all) {
        printf("transit\n");
    } else {
         if ((req->mode & m_tram)      == m_tram)      printf("tram,");
         if ((req->mode & m_subway)    == m_subway)    printf("subway,");
         if ((req->mode & m_rail)      == m_rail)      printf("rail,");
         if ((req->mode & m_bus)       == m_bus)       printf("bus,");
         if ((req->mode & m_ferry)     == m_ferry)     printf("ferry,");
         if ((req->mode & m_cablecar)  == m_cablecar)  printf("cablecar,");
         if ((req->mode & m_gondola)   == m_gondola)   printf("gondola,");
         if ((req->mode & m_funicular) == m_funicular) printf("funicular,");
         printf("\b\n");
    }
}

time_t req_to_date (router_request_t *req, tdata_t *tdata, struct tm *tm_out) {
    calendar_t day_mask = req->day_mask;
    uint8_t cal_day = 0;
    while (day_mask >>= 1) cal_day++;

    time_t seconds = tdata->calendar_start_time + (cal_day * SEC_IN_ONE_DAY) - ((tdata->dst_active & 1 << cal_day) ? SEC_IN_ONE_HOUR : 0);
    localtime_r(&seconds, tm_out);

    return seconds;
}

time_t req_to_epoch (router_request_t *req, tdata_t *tdata, struct tm *tm_out) {
    calendar_t day_mask = req->day_mask;
    uint8_t cal_day = 0;
    while (day_mask >>= 1) cal_day++;

    time_t seconds = tdata->calendar_start_time + (cal_day * SEC_IN_ONE_DAY) + RTIME_TO_SEC(req->time - RTIME_ONE_DAY) - ((tdata->dst_active & 1 << cal_day) ? SEC_IN_ONE_HOUR : 0);
    localtime_r(&seconds, tm_out);

    return seconds;
}
