/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* profile.c */

#include "config.h"
#include "tdata.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define N_QUANTILES 5
#define MAX_STATES (10 * 1024 * 1024)

/* 2^16 ~= 18.2 hours in seconds. */

// control debug output
#define P for(;0;)

// where we are trying to reach
static uint32_t target_stop;

// Bits for the days over which the analysis is performed.
static calendar_t day_mask;
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
    s->min = UNREACHED;
    s->max = 0;
    s->mean = 0;
}

static void stats_print (struct stats *s) {
    printf ("[%3d %2.1f %3d]", s->min, s->mean, s->max);
}

struct state {
    struct state *back_state; // could actually be an int index into the pool
    uint32_t stop;            // use route_stop instead? global stop can be found from it, but not other way round.
    uint32_t back_route;      // WALK for states that result from transfers
    struct stats stats;       //cumulative stats since the beginning of the route
};

static void state_init (struct state *state) {
    state->back_state = NULL;
    state->stop = NONE;
    state->back_route = NONE;
    stats_init (&(state->stats));
}

static void state_print (struct state *state) {
    stats_print (&(state->stats));
    uint32_t last_stop = state->back_state->stop;
    uint32_t this_stop = state->stop;

    char *last_stop_string = tdata_stop_name_for_index (&tdata, last_stop);
    char *this_stop_string = tdata_stop_name_for_index (&tdata, this_stop);
    char *route_shortname = tdata_shortname_for_route (&tdata, state->back_route);
    char *route_headsign = tdata_headsign_for_route (&tdata, state->back_route);
    printf (" FROM %s [%d] TO %s [%d] VIA %s %s [%d]\n", last_stop_string, last_stop, this_stop_string, this_stop,
        route_shortname, route_headsign, state->back_route);
}

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

static void path_print (struct state *state) {
    while (state != NULL && state->back_state != NULL) {
        state_print (state);
        state = state->back_state;
    }
}

/* Explore the given route starting at the given stop. Add a state at each stop with transfers. */
static void explore_route (struct state *state, uint32_t route_idx, uint32_t stop_idx, rtime_t transfer_time) {
    P printf ("    exploring route %s %s [%d] toward X from stop %s [%d]\n",
        tdata_shortname_for_route (&tdata, route_idx),  tdata_headsign_for_route (&tdata, route_idx),
        route_idx, tdata_stop_name_for_index (&tdata, stop_idx), stop_idx);
    rrrr_route_t route = tdata.routes[route_idx];
    uint32_t *route_stops = tdata_stops_for_route (&tdata, route_idx);
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
        // Only create states at stops that have transfers.
        if (tdata.stops[this_stop_idx].transfers_offset == tdata.stops[this_stop_idx + 1].transfers_offset) continue;
        // Only create states at stops that do not appear in the existing chain of states.
        if (path_has_stop (state, this_stop_idx)) continue;
        // printf ("      enqueueing state at stop %s [%d]\n", tdata_stop_name_for_index (&tdata, this_stop_idx), this_stop_idx);
        struct state *new_state = states_next ();
        new_state->stop = this_stop_idx;
        new_state->back_state = state;
        new_state->back_route = route_idx;
        new_state->stats = route_stats[route.route_stops_offset + s]; // copy stats for current stop in current route
        stats_subtract (&(new_state->stats), stats0);                 // relativize times to board location
        stats_add      (&(new_state->stats), &(state->stats));        // accumulate stats from previous state
        if (this_stop_idx == target_stop) {
            printf ("hit target.\n");
            path_print (new_state);
        }
    }
    // alternate iteration method
    // while (*curr_stop != state->stop) ++curr_stop;
    // while (curr_stop <= end_stop) {
}

/* Loop over all routes calling at this stop, exploring each route. TODO: Skip all used routes or their reversed "twins". */
static void explore_stop (struct state *state, uint32_t stop_idx, rtime_t transfer_time) {
    P printf ("  exploring routes calling at stop %s [%d]\n", tdata_stop_name_for_index (&tdata, stop_idx), stop_idx);
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
    P printf ("exploring transfers from stop %s [%d]\n", tdata_stop_name_for_index (&tdata, state->stop), state->stop);
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

/* Analyze every route, finding the min, avg, and max travel time at each stop.
   The times are cumulative along the route, and relative to the first departure.
   The result should be the concatenation of a 'struct stats' array of length route.n_stops for each route,
   in order of route index. This ragged array has the same general form as tdata.route_stops.  */
void compute_route_stats () {
    {
        int n_stops_all_routes = 0; // Sum(route.n_stops) over all routes. Each stop may appear in more than one route.
        for (uint32_t r = 0; r < tdata.n_routes; ++r) n_stops_all_routes += tdata.routes[r].n_stops;
        route_stats = malloc (n_stops_all_routes * sizeof(struct stats));
    }

    printf ("computing route/stop travel time stats...\n");
    int route_first_index = 0;

    /* For each trip in each route, iterate over the stops updating the per_stop stats. */
    /* Many trips encountered are exact duplicates (TimeDemandTypes). */
    /* Actually we probably need separate departures stats for relativizing. */
    for (uint32_t r = 0; r < tdata.n_routes; ++r) {
        rrrr_route_t route = tdata.routes[r];
        for (int s = 0; s < route.n_stops; ++s) {
            stats_init (&(route_stats[route_first_index + s]));
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

int main () {

    /* Load timetable data and summarize routes and transfers */
    tdata_load (RRRR_INPUT_FILE, &tdata);
    compute_route_stats ();    // also allocates static array for route stats
    compute_transfer_stats (); // also allocates static array for transfer stats

    /* Allocate router scratch space */
    states = malloc (sizeof(struct state) * MAX_STATES);
    stop_stats = malloc (sizeof(struct stats) * tdata.n_stops);

    struct state *state = states_next ();
    state_init (state);
    state->stop = 1200;
    target_stop = 1344;
    while (states_head < states_tail) {
        explore_transfers (states + states_head);
        ++states_head;
    }

    /* Free static arrays*/
    free (route_stats);
    free (transfer_stats);
    free (states);
    free (stop_stats);

}
