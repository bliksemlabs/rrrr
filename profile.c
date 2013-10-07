/* profile.c */

#include "config.h"
#include "tdata.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define N_QUANTILES 5

/* 2^16 ~= 18.2 hours in seconds. */

// control debug output
#define P for(;0;)

struct stats {
    rtime_t quantiles[N_QUANTILES];
    //rtime_t mean;
    //rtime_t stddev; // cannot accumulate/subtract
};

struct state {
    struct state *back_state; // could actually be an int index into the pool
    uint32_t stop; // maybe store route_stop instead since global stop can be found from it, but not other way
    uint32_t back_route;
    struct stats stats;
    //uint8_t  n_transfers;
};

/* Subtracts b out of a element-wise in place. */
static void subtract_stats (struct stats *a, struct stats *b) {
    for (int i = 0; i < N_QUANTILES; ++i) a->quantiles[i] -= b->quantiles[i];
}

/* Adds b into a element-wise in place. */
static void add_stats (struct stats *a, struct stats *b) {
    for (int i = 0; i < N_QUANTILES; ++i) a->quantiles[i] += b->quantiles[i];
}

static void state_init (struct state *state) {
    state->stop = NONE;
    state->back_state = NULL;
    state->back_route = NONE;
    for (int i = 0; i < N_QUANTILES; ++i) state->stats.quantiles[i] = 0;
}

// Bits for the days over which the analysis is performed.
uint32_t day_mask;
struct stats *route_stats = NULL;

tdata_t tdata;

// This array of states is a pre-allocated pool that also serves as a queue. This is basically a brute force search.
#define MAX_STATES (100 * 1024 * 1024)
struct state *states; // malloc to avoid relocation errors
// these should probably be pointers
uint32_t states_head = 0; // the first unexplored state
uint32_t states_tail = 0; // the first unused state; this is also the number of states

// where we are trying to reach
static uint32_t target_stop;

/*
// store best min/average/max time to reach each stop, and prune some new states on basis of worst time.
struct state best_times[n_stops];
*/

static struct state *states_next () {
    if (states_tail >= MAX_STATES) exit (0);
    struct state *ret = &(states[states_tail]);
    states_tail += 1;
    P if (states_tail % 10000 == 0) printf ("number of states is now %d\n", states_tail);
    return ret;
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
        if (route_idx == state->back_route) return true;
        state = state->back_state;
    }
    return false;
}

static void path_dump (struct state *state) {
    while (state != NULL) {
        printf ("stop %s via route %s \n", tdata_stop_desc_for_index (&tdata, state->stop), tdata_route_desc_for_index (&tdata, state->back_route));
        state = state->back_state;
    }    
}

/* Explore the given route starting at the given stop. Add a state at each stop with transfers. */
static void explore_route (struct state *state, uint32_t route_idx, uint32_t stop_idx, rtime_t transfer_time) {
    P printf ("    exploring route %s [%d] toward X from stop %s [%d]\n", 
        tdata_route_desc_for_index (&tdata, route_idx), 
        route_idx, tdata_stop_desc_for_index (&tdata, stop_idx), stop_idx);
    route_t route = tdata.routes[route_idx];
    uint32_t *route_stops = tdata_stops_for_route (tdata, route_idx);
    struct stats *stats0;
    bool onboard = false;
    for (int s = 0; s < route.n_stops; ++s) {
        uint32_t this_stop_idx = route_stops[s];
        if (!onboard && this_stop_idx == stop_idx) {
            onboard = true;
            stats0 = &(route_stats[route.route_stops_offset + s]);
            continue;
        }
        if (!onboard) continue;
        // Only create states at stops that have transfers.
        if (tdata.stops[this_stop_idx].transfers_offset == tdata.stops[this_stop_idx + 1].transfers_offset) continue;
        // Only create states at stops that do not appear in the existing chain of states.
        if (path_has_stop (state, this_stop_idx)) continue;
        // printf ("      enqueueing state at stop %s [%d]\n", tdata_stop_desc_for_index (&tdata, this_stop_idx), this_stop_idx);
        struct state *new_state = states_next ();
        new_state->stop = this_stop_idx;
        new_state->back_state = state;
        new_state->back_route = route_idx;
        new_state->stats = route_stats[route.route_stops_offset + s]; // copy stats for current stop in current route
        subtract_stats (&(new_state->stats), stats0); // relativize times to board location
        add_stats (&(new_state->stats), &(state->stats)); // accumulate stats from previous state
        if (this_stop_idx == target_stop) {
            printf ("hit target.\n");
            path_dump (new_state);
        }
    }
    // alternate iteration method
    // while (*curr_stop != state->stop) ++curr_stop;
    // while (curr_stop <= end_stop) {
}

/* Loop over all routes calling at this stop, exploring each route. TODO: Skip all used routes or their reversed "twins". */ 
static void explore_stop (struct state *state, uint32_t stop_idx, rtime_t transfer_time) {
    P printf ("  exploring routes calling at stop %s [%d]\n", tdata_stop_desc_for_index (&tdata, stop_idx), stop_idx);
    uint32_t *routes;
    uint32_t n_routes = tdata_routes_for_stop (&tdata, stop_idx, &routes);
    for (uint32_t r = 0; r < n_routes; ++r) {
        uint32_t route_idx = routes[r];
        // Avoid boarding a previously used route.
        if (path_has_route (state, route_idx)) continue;
        explore_route (state, route_idx, stop_idx, transfer_time);
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
        //if (stop_index_to == state->back_state->stop) continue; // no taking the same route
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

/* Computes quantiles for the given array of times. Has the side-effect of sorting the input array. */
static void quantiles (rtime_t *times, uint32_t n, struct stats *stats) {
    /* Sort the arrival time array for this stop. */
    qsort (times, n, sizeof(rtime_t), (__compar_fn_t) rtime_compare);
    // printf ("computing quantiles for %d times: \n", n);
    // for (int i = 0; i < n; ++i) printf ("%d ", times[i]);
    // printf ("\n");
    for (int i = 0; i < N_QUANTILES; ++i) {
        double q = ((double)i) / (N_QUANTILES - 1);
        double qi = quantile (times, n, q);
        // printf ("q=%0.2f qi=%f \n", q, qi);
        stats->quantiles[i] = qi;
        // printf ("%4d ", stats->quantiles[i]);
    }
    // printf ("\n");
}

/* Analyze every route, finding the min, avg, and max travel time at each stop. (actually now it's quantiles)
   The times are cumulative along the route, and relative to the first departure.
   The result should be the concatenation of a 'struct stats' array of length route.n_stops for each route, 
   in order of route index. This ragged array has the same general form as tdata.route_stops.  */
void compute_route_stats () {
    int total_n_stops = 0;
    for (int r = 0; r < tdata.n_routes; ++r) total_n_stops += tdata.routes[r].n_stops;
    /* static */ route_stats = malloc (total_n_stops * sizeof(struct stats));
    
    printf ("computing travel time quantiles...\n");
    uint32_t route_begin_index = 0;
    
    /* Foreach route, find arrival time quantiles at each stop. */
    for (int r = 0; r < tdata.n_routes; ++r) {
        route_t route = tdata.routes[r];
        rtime_t arrivals[route.n_stops][route.n_trips];
        /* Foreach trip in route, expand arrivals into stop-major rows. Many duplicate entries (timedemand). */
        for (int t = 0; t < route.n_trips; ++t) {
            /* Get the raw zero-based TimeDemandType stoptimes. */
            stoptime_t *stoptimes = tdata_timedemand_type (&tdata, r, t);
            for (int s = 0; s < route.n_stops; ++s) {
                arrivals[s][t] = stoptimes[s].arrival;
            }
        }
        for (int s = 0; s < route.n_stops; ++s) {
            quantiles (&(arrivals[s][0]), route.n_trips, &(route_stats[route_begin_index + s])); // OR tdata.route_stops_offset
        }
        route_begin_index += route.n_stops;
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

int main () {
    tdata_load (RRRR_INPUT_FILE, &tdata);
    compute_route_stats ();
    states = malloc (sizeof(struct state) * MAX_STATES);
    struct state *state = states_next ();
    state_init (state);
    state->stop = 1200;
    target_stop = 1344;
    while (states_head < states_tail) {
        explore_transfers (states + states_head);
        ++states_head;
    }
}
