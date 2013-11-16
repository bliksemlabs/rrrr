/* profile.c */

#include "config.h"
#include "tdata.h"
#include "json.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define N_QUANTILES 5
#define MAX_STATES (20 * 1024 * 1024)
#define MAX_ROUNDS 3

/* 2^16 ~= 18.2 hours in seconds. */

// control debug output
#define P for(;0;)

// where we are trying to reach
static uint32_t target_stop;

// Bits for the days over which the analysis is performed.
static uint32_t day_mask;
static struct stats *route_stats = NULL; 
static struct stats *transfer_stats = NULL;
static struct stats *stop_stats; // store best known for each stop, prune some new states on basis of worst time.
static tdata_t tdata;

// A pre-allocated pool of states that also serves as a queue.
static struct state *states; // malloc to avoid relocation errors
static uint32_t states_head = 0; // the first unexplored state (these should probably be pointers)
static uint32_t states_tail = 0; // the first unused state; this is also the number of states

//  Should probably add a per-route "frequency" as well. Freq(variant1 U variant2) is freq(v1) + freq(v2), freq(leg1, leg2) = min(freq(l1), freq(l2)).
//  Then there can be an option to merge variants.

// Store lists of states at each stop, allows merging states from the same route.
static struct state **stop_states;

/* 
  Summary statistics over all trips on a given route at a given stop.
  These must be additive functions because legs are summed to give stats for an entire itinerary.  
*/
struct stats {
    rtime_t min;
    rtime_t max;
    float mean;
};

/* Subtracts b out of a element-wise in place. This is valid because all our summary statistics are additive functions. */
static void stats_subtract (struct stats *a, struct stats *b) {
    a->min  -= b->min;
    a->max  -= b->max;
    a->mean -= b->mean;
}

/* Adds b into a element-wise in place. This is valid because all our summary statistics are additive functions. */
static void stats_add (struct stats *a, struct stats *b) {
    a->min  += b->min;
    a->max  += b->max;
    a->mean += b->mean;
}

static void stats_init (struct stats *s) {
    s->min  = 0;
    s->max  = 0;
    s->mean = 0;
}

static void stats_set (struct stats *s, rtime_t value) {
    s->min  = value;
    s->max  = value;
    s->mean = value;
}

static void stats_print (struct stats *s) {
    printf ("[%3.0f %3.0f %3.0f]", RTIME_TO_SEC(s->min) / 60.0, RTIME_TO_SEC(s->mean) / 60.0, RTIME_TO_SEC(s->max) / 60.0);
}

struct state {
    struct state *back_state; // could actually be an int index into the pool
    struct state *next;       // the next saved state at the same stop (forms a linked list)
    uint32_t stop;            // use route_stop instead? global stop can be found from it, but not other way round.
    uint32_t route;           // next route for states that result from transfers, back route for states resulting from rides.
    struct stats stats;       // cumulative stats since the beginning of the route
    bool transfer;            // true if this edge is the result of a transfer, false if it is from a transit ride
};

static void state_init (struct state *state) {
    state->back_state = NULL;
    state->next = NULL;
    state->stop = NONE;
    state->route = NONE;
    stats_init (&(state->stats));
    state->transfer = false;
}

static void state_print (struct state *state) {
    stats_print (&(state->stats));
    uint32_t last_stop = state->back_state->stop;
    uint32_t this_stop = state->stop;
    char *last_stop_string = tdata_stop_desc_for_index (&tdata, last_stop);
    char *this_stop_string = tdata_stop_desc_for_index (&tdata, this_stop); 
    char *route_string = tdata_route_desc_for_index (&tdata, state->route);
    printf (" %s %s [%d] FROM %s [%d] TO %s [%d]\n", state->transfer ? "XFER" : "RIDE", route_string, state->route, 
        last_stop_string, last_stop, this_stop_string, this_stop);
}

/*
static void die (char *message) { //, int exit_code) {
    printf ("%s\n", message);
    exit (1);
}
*/

static struct state *states_next () {
    if (states_tail >= MAX_STATES) die ("exceeded maximum number of states");
    struct state *ret = &(states[states_tail]);
    states_tail += 1;
    P if (states_tail % 10000 == 0) printf ("number of states is now %d\n", states_tail);
    return ret;
}

/* Cancel allocation of the topmost state on the stack. */
static void states_pop () {
    states_tail -= 1;
}

static void states_reset () {
    states_head = 0;
    states_tail = 0;
}

static bool path_has_stop (struct state *state, uint32_t stop_idx) {
    while (state != NULL) {
        if (stop_idx == state->stop) return true;
        state = state->back_state;
    }    
    return false;
}

static bool path_has_route (struct state *state, uint32_t route_idx) {
    while (state != NULL) {
        if (route_idx == state->route) return true;
        state = state->back_state;
    }
    return false;
}

static void path_print (struct state *state) {
    while (state != NULL && state->back_state != NULL) {
        state_print (state);
        state = state->back_state;
    }    
    printf ("\n");
}

struct ride {
    uint32_t from_stop;
    uint32_t to_stop;
    uint32_t route;
    uint32_t xfer_stop;
    struct stats stats;
    struct stats xfer_stats;
};

static void json_stats (struct stats *stats, char *key) {
    json_key_obj (key);
        json_kd ("min", stats->min);
        json_kd ("avg", stats->mean);
        json_kd ("max", stats->max);
        json_kf ("freq", 1.0);
    json_end_obj ();
}

static void ride_render_json (struct ride *ride) {
    char *route_string = tdata_route_desc_for_index (&tdata, ride->route);
    json_obj(); /* one ride */
        json_kv ("route", route_string);
        json_kv ("toward", NULL);
        json_kv ("qualifier", NULL);
        json_key_arr ("stops");
        {
            route_t route = tdata.routes[ride->route];
            uint32_t *route_stops = tdata_stops_for_route (tdata, ride->route);
            bool onboard = false;
            for (int s = 0; s < route.n_stops; ++s) {
                uint32_t stop_idx = route_stops[s];
                if (!onboard) {
                    if (stop_idx == ride->from_stop) onboard = true;
                    else continue;
                }
                char *stop_id = tdata_stop_id_for_index (&tdata, stop_idx);
                json_string (stop_id);
                if (stop_idx == ride->to_stop) break;
            }
        }
        json_end_arr ();
        json_stats (&(ride->stats), "times");
        if (ride->xfer_stop != NONE) {
            json_stats (&(ride->xfer_stats), "transfer_times");
        }
    json_end_obj ();
}

static void path_render_json (struct state *state) {
    struct ride rides[MAX_ROUNDS];
    int n_rides = 0;
    struct state *xfer_state = NULL;
    while (state != NULL && state->back_state != NULL) {
        struct ride *ride = rides + n_rides;
        ride->from_stop = state->back_state->stop;
        ride->to_stop = state->stop;
        ride->route = state->route;
        if (xfer_state != NULL) {
            ride->xfer_stop = xfer_state->stop;
            ride->xfer_stats = xfer_state->stats;
            stats_subtract (&(ride->xfer_stats), &(state->stats));
        } else {
            ride->xfer_stop = NONE;
            stats_init (&(ride->xfer_stats));
        }
        ride->stats = state->stats;
        stats_subtract (&(ride->stats), &(state->back_state->stats));
        xfer_state = state->back_state;
        state = xfer_state->back_state;
        n_rides += 1;
    }
    json_obj (); /* one route option */
        // TODO make summary string    for (int n = n_rides - 1; n >= 0; --n) {        
        json_kv ("summary", "ABC");
        json_stats (&(state->stats), "times");
        json_key_arr ("rides");
            for (int n = n_rides - 1; n >= 0; --n) {        
                ride_render_json (rides + n);
            }
        json_end_arr ();    
    json_end_obj ();        
}

static void route_transfer_print (route_transfer_t *rt) {
    printf ("  route transfer to stop %d dist %d from route %d to route %d\n", rt->to_stop_idx, rt->dist_meters, rt->from_route_idx, rt->to_route_idx);
}

/* Loop over all route-transfers from the given state, and enqueue a transfer state for each useful target. 
   Returns the number of new transfer states added to the queue. */
static int explore_route_transfers (struct state *state) {
    int n_added = 0;
    P printf ("exploring route transfers from stop %s [%d] on route %s [%d]\n", 
        tdata_stop_desc_for_index (&tdata, state->stop), state->stop,
        tdata_route_desc_for_index (&tdata, state->route), state->route);
    if (state->transfer) die ("route transfers should not be explored from a transfer state.");
    uint32_t from_stop_idx  = state->stop;
    uint32_t from_route_idx = state->route;
    uint32_t toff0 = tdata.route_transfers_offsets[from_stop_idx];
    uint32_t toff1 = tdata.route_transfers_offsets[from_stop_idx + 1];
    for (uint32_t t = toff0; t < toff1; ++t) {
        route_transfer_t rt = tdata.route_transfers[t];
        P route_transfer_print (&rt);
        if (rt.from_route_idx != from_route_idx) continue;
        // Avoid boarding a previously used route.
        if (path_has_route (state, rt.to_route_idx)) continue;
        P printf ("  xfer: exploring route %s [%d] calling at stop %s [%d]\n", 
            tdata_route_desc_for_index (&tdata, rt.to_route_idx), rt.to_route_idx,
            tdata_stop_desc_for_index  (&tdata, rt.to_stop_idx),  rt.to_stop_idx);       
        struct state *new_state = states_next ();
        new_state->back_state = state;
        new_state->next = NULL;
        new_state->stop = rt.to_stop_idx;
        new_state->route = rt.to_route_idx;
        new_state->transfer = true;
        stats_set (&(new_state->stats), 30); // TODO update stats properly             
        stats_add (&(new_state->stats), &(state->stats));
        ++n_added;
    }
    return n_added;
}

/* Returns whether the routes used in the new path are a superset of those used in the old path, respecting order. */
static bool routes_superset (struct state *old, struct state *new) {
    while (old != NULL) {
        char *old_route_name = tdata_route_desc_for_index (&tdata, old->route);
        while (true) {
            char *new_route_name = tdata_route_desc_for_index (&tdata, new->route);
            if (strcmp (old_route_name, new_route_name) == 0) break;
            new = new->back_state;
            if (new == NULL) return false;
        }
        old = old->back_state;
    }
    return true;
}

/* Check whether the routes used in the new state are a superset of those used in any state at the same stop. */
static bool routes_superset_any (struct state *new) {
    struct state *old = stop_states[new->stop];
    while (old != NULL) {
        if (routes_superset (old, new)) return true;
        old = old->next;        
    }
    return false;
}

/* Returns true if the state is useless and therefore should not be registered at the stop for further exploration. */
// maybe instead check if all routes + transfer points are the same
static bool state_subsumed (struct state *new) {
    // only apply to ride states, not transfer states
    struct state *old = stop_states[new->stop];
    while (old != NULL) {
        // check that both states come from the same previous leg (and therefore the back-stop is the same)
        // this should also catch different rides on the same route from different back stops on the 0th leg (back-back state is initial)
        if (old->back_state != NULL && new->back_state != NULL &&
            old->back_state->back_state == new->back_state->back_state) {
            char *old_route_name = tdata_route_desc_for_index (&tdata, old->route);
            char *new_route_name = tdata_route_desc_for_index (&tdata, new->route);
            if (strcmp (old_route_name, new_route_name) == 0) return true; // semantic equality, each route string seems to be stored separately not deduplicated
            //if (old_route_name == new_route_name) return true; // identity equality, depends on deduplicated strings
        }
        old = old->next;
    }
    // no states were registered at this stop, or those that were did not subsume the new one.
    return false;
}

/* Explore the given route starting at the given stop. Add a state at each stop with transfers. */
static void explore_route (struct state *state) {
    // aliases, not necessary
    uint32_t route_idx = state->route;
    uint32_t stop_idx = state->stop;
    P printf ("    exploring route %s [%d] toward X from stop %s [%d]\n", 
        tdata_route_desc_for_index (&tdata, route_idx), 
        route_idx, tdata_stop_desc_for_index (&tdata, stop_idx), stop_idx);
    if ( ! state->transfer) die ("routes should only be scanned based on transfer states.");
    route_t route = tdata.routes[route_idx];
    uint32_t *route_stops = tdata_stops_for_route (tdata, route_idx);
    struct stats *stats0;
    bool onboard = false;
    for (int s = 0; s < route.n_stops; ++s) {
        uint32_t this_stop_idx = route_stops[s];
        if (!onboard) {
            if (this_stop_idx == stop_idx) {
                onboard = true;
                stats0 = &(route_stats[route.route_stops_offset + s]);
            }
            continue;
        }
        // Only create states at stops that do not appear in the existing chain of states.
        if (path_has_stop (state, this_stop_idx)) continue;
        struct state *new_state = states_next ();
        new_state->back_state = state;
        new_state->next = NULL;
        new_state->stop = this_stop_idx;
        new_state->route = route_idx;
        new_state->stats = route_stats[route.route_stops_offset + s]; // copy stats for current stop in current route
        stats_subtract (&(new_state->stats), stats0);                 // relativize times to board location
        stats_add      (&(new_state->stats), &(state->stats));        // accumulate stats from previous state
        if (state_subsumed (new_state) || routes_superset_any (new_state)) {
            states_pop ();
            continue;
        } else if (this_stop_idx == target_stop) {
            // only check whether target is reached if the state is not "useless". do not explore transfers from target.
            path_render_json (new_state);
            path_print (new_state);
        } else if ( ! explore_route_transfers (new_state)) {
            states_pop (); // if no transfer states were added, the ride state is not needed.
            continue;
        }
        // register the state at its stop by adding it to the head of the linked list
        new_state->next = stop_states[new_state->stop];
        stop_states[new_state->stop] = new_state;
    }
    // alternate iteration method
    // while (*curr_stop != state->stop) ++curr_stop;
    // while (curr_stop <= end_stop) {
}

/* Loop over all routes calling at this stop, enqueuing a new transfer state for each route. */ 
static void explore_stop (struct state *state, uint32_t stop_idx, rtime_t transfer_time) {
    P printf ("  exploring routes calling at stop %s [%d]\n", tdata_stop_desc_for_index (&tdata, stop_idx), stop_idx);
    uint32_t *routes;
    uint32_t n_routes = tdata_routes_for_stop (&tdata, stop_idx, &routes);
    for (uint32_t r = 0; r < n_routes; ++r) {
        uint32_t route_idx = routes[r];
        // Avoid boarding a previously used route. TODO: avoid routes' reversed "twins"
        if (path_has_route (state, route_idx)) continue;
        struct state *new_state = states_next ();
        new_state->back_state = state;
        new_state->next = NULL;
        new_state->stop = stop_idx;
        new_state->route = route_idx;
        new_state->transfer = true;
        new_state->stats = state->stats; // TODO update stats properly
    }
}

/* Loop over all transfers from the given stop, and explore_stop at each target stop including the given one. */
static void explore_transfers (struct state *state) {
    P printf ("exploring transfers from stop %s [%d]\n", tdata_stop_desc_for_index (&tdata, state->stop), state->stop);
    uint32_t stop_index_from = state->stop;
    uint32_t t  = tdata.stops[stop_index_from    ].transfers_offset;
    uint32_t tN = tdata.stops[stop_index_from + 1].transfers_offset;        
    for ( ; t < tN ; ++t) {
        uint32_t stop_index_to = tdata.transfer_target_stops[t];
        rtime_t  transfer_time = tdata.transfer_dist_meters[t] << 4; // in meters / seconds, not rtime
        explore_stop (state, stop_index_to, transfer_time);
    }
    explore_stop (state, stop_index_from, 0); 
}

static int rtime_compare (const rtime_t *a, const rtime_t *b) {
    if (*a == *b) return 0;
    if (*a  < *b) return -1;
    else return 1;
}

static double quantile (rtime_t *times, uint32_t n, double q) {
    if (q < 0 || q > 1 || n < 1) return UNREACHED;
    // index of Q0 is 0; index of Q100 is n-1.
    uint32_t i100 = n - 1; // index could be 0 if there is only one element.    
    double fractional_index = q * (i100);
    uint32_t integral_index = fractional_index; // truncate
    //printf ("fractional index = %f, integral index = %d ", fractional_index, integral_index);
    fractional_index -= integral_index;
    if (fractional_index <= 0) return times[integral_index];
    /* Linear interpolation for fractional indexes. */
    uint32_t v0 = times[integral_index    ];
    uint32_t v1 = times[integral_index + 1];
    return v0 + (v1 - v0) * fractional_index;
}

/* Analyze every route, finding the min, avg, and max travel time at each stop.
   The times are cumulative along the route, and relative to the first departure.
   The result should be the concatenation of a 'struct stats' array of length route.n_stops for each route, 
   in order of route index. This ragged array has the same general form as tdata.route_stops.  */
void compute_route_stats () {
    {
        int n_stops_all_routes = 0; // Sum(route.n_stops) over all routes. Each stop may appear in more than one route.
        for (int r = 0; r < tdata.n_routes; ++r) n_stops_all_routes += tdata.routes[r].n_stops;
        route_stats = malloc (n_stops_all_routes * sizeof(struct stats));
    }
    
    printf ("computing route/stop travel time stats...\n");
    int route_first_index = 0;
    
    /* For each trip in each route, iterate over the stops updating the per_stop stats. */
    /* Many trips encountered are exact duplicates (TimeDemandTypes). */
    /* Actually we probably need separate departures stats for relativizing. */
    for (int r = 0; r < tdata.n_routes; ++r) {
        route_t route = tdata.routes[r];
        for (int s = 0; s < route.n_stops; ++s) {
            struct stats *stats = &(route_stats[route_first_index + s]);
            stats_init (stats);
            stats->min = UNREACHED; // because we need a number bigger than all others to compare against
        }
        for (int t = 0; t < route.n_trips; ++t) {
            stoptime_t *stoptimes = tdata_timedemand_type (&tdata, r, t); // Raw zero-based TimeDemandType stoptimes.
            for (int s = 0; s < route.n_stops; ++s) {
                struct stats *stats = &(route_stats[route_first_index + s]);
                rtime_t arrival = stoptimes[s].arrival;
                if (arrival < stats->min) stats->min = arrival;
                if (arrival > stats->max) stats->max = arrival;
                stats->mean += arrival;
            }
        }
        for (int s = 0; s < route.n_stops; ++s) {
            route_stats[route_first_index + s].mean /= route.n_trips;
        }
        route_first_index += route.n_stops;
    }    
}

/* Compute stats for one route-route transfer on the fly, respecting the distance-based minimum transfer time. */
// there are probably too many transfers though, between all pairs of stops rather than between each pair of routes.
// upon transferring to another route, we want only the "best" one to be considered, like in raptor.
static void profile_transfer (uint32_t route0, uint32_t stop0, uint32_t route1, uint32_t stop1, struct stats *stats) {
    // choose one service day
    // loop over stops in second route (in case target stop appears more than once)
    //   loop over arrival times on route 0 at stop 0
    //     step forward in arrival times on route 1 at stop 1 looking for departure >= arrival + xfer time
    //     record transfer time in array of length (ntrips in route0)
    // 
}

void compute_transfer_stats () {
    transfer_stats = malloc (tdata.n_stops * sizeof(struct stats));
}

int main (int argc, char **argv) {

    if (argc != 3) die ("command [from] [to]");

    /* Load timetable data and summarize routes and transfers */
    tdata_load (RRRR_INPUT_FILE, &tdata);
    compute_route_stats ();    // also allocates static array for route stats
    compute_transfer_stats (); // also allocates static array for transfer stats

    /* Allocate router scratch space */
    states = malloc (sizeof(struct state) * MAX_STATES);
    stop_stats =  malloc (sizeof (struct stats) * tdata.n_stops);
    stop_states = malloc (sizeof (struct state*) * tdata.n_stops);

    struct state *state = states_next ();
    state_init (state);
    state->stop = atoi(argv[1]);
    target_stop = atoi(argv[2]);

    /* Begin JSON output */
    size_t buflen = 1024 * 1024;
    char buf[buflen];
    json_begin (buf, buflen);
    json_obj ();
    json_key_arr ("options");
    
    /* For the first state, explore all transfers. For subsequent ones, explore only route-route transfers. */
    explore_transfers (states + states_head++);
    int round = 0;
    uint32_t states_round_end = states_tail;
    while (states_head < states_tail) {
        if (states_head == states_round_end) {
            states_round_end = states_tail;
            if (++round == MAX_ROUNDS) {
                printf ("ending search at %d rounds. total states explored: %d\n", MAX_ROUNDS, states_head); 
                break;
            }
        }
        struct state *state = states + states_head++;
        if (state->transfer) explore_route (state); // state is result of a transfer, scan its route.
        /* State that are the result of rides are explored when they are created, while scanning the route. */
    }

    /* Finish JSON output */
    json_end_arr ();
    json_end_obj ();
    json_dump ();
    
    /* Free static arrays */
    free (route_stats);
    free (transfer_stats);
    free (states);
    free (stop_stats);

}

