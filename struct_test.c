#include <stdio.h>
#include <stdint.h>
typedef uint16_t rtime_t;

/* Demonstrating how order affects struct size */

struct s0 {
    rtime_t time;        // The time when this stop was reached
    uint32_t back_stop;  // The index of the previous stop in the itinerary
    uint32_t back_route; // The index of the route used to travel from back_stop to here, or WALK
    uint32_t back_trip;  // The index of the trip used to travel from back_stop to here, or WALK
    rtime_t board_time;  // The time at which the trip within back_route left back_stop
    char *back_trip_id;  // A text description of the trip used within back_route
};

struct s1 {
    char *back_trip_id;  // A text description of the trip used within back_route
    uint32_t back_stop;  // The index of the previous stop in the itinerary
    uint32_t back_route; // The index of the route used to travel from back_stop to here, or WALK
    uint32_t back_trip;  // The index of the trip used to travel from back_stop to here, or WALK
    rtime_t board_time;  // The time at which the trip within back_route left back_stop
    rtime_t time;        // The time when this stop was reached
};

struct s2 {
    uint32_t back_stop;  // The index of the previous stop in the itinerary
    uint32_t back_route; // The index of the route used to travel from back_stop to here, or WALK
    uint32_t back_trip;  // The index of the trip used to travel from back_stop to here, or WALK
    uint32_t back_stop_walk;
    rtime_t board_time;  // The time at which the trip within back_route left back_stop
    rtime_t time;        // The time when this stop was reached
};

struct s3 {
    uint32_t back_stop;  // The index of the previous stop in the itinerary
    uint32_t back_route; // The index of the route used to travel from back_stop to here, or WALK
    uint32_t back_trip;  // The index of the trip used to travel from back_stop to here, or WALK
    rtime_t  time;       // The time when this stop was reached
    rtime_t  board_time; // The time at which the trip within back_route left back_stop
    /* Second phase footpath/transfer results */
    uint32_t walk_stop;  // The stop from which this stop was reached by walking (2nd phase)
    rtime_t  walk_time;  // The time when this stop was reached by walking (2nd phase)
};

struct s4 {
    /* First phase: transit ride results */
    /* Second phase: transfer results */
    uint32_t back_stop;  // The index of the previous stop in the itinerary
    uint32_t back_route; // The index of the route used to travel from back_stop to here, or WALK
    uint32_t back_trip;  // The index of the trip used to travel from back_stop to here, or WALK
    uint32_t walk_from;  // The stop from which this stop was reached by walking (2nd phase)
    rtime_t  time;       // The time when this stop was reached
    rtime_t  walk_time;  // The time when this stop was reached by walking (2nd phase)
};

struct t0 {
    uint32_t target_stop;
    float dist_meters;
};

struct t1 {
    uint8_t  dist_meters;
    uint32_t target_stop;
};

int main () {
    int n_stops = 60000;
    int n_rounds = 5;
    printf ("s0 size = %ld, total size = %ld \n", sizeof(struct s0), n_stops * n_rounds * sizeof(struct s0));
    printf ("s1 size = %ld, total size = %ld \n", sizeof(struct s1), n_stops * n_rounds * sizeof(struct s1));
    printf ("s2 size = %ld, total size = %ld \n", sizeof(struct s2), n_stops * n_rounds * sizeof(struct s2));
    printf ("s3 size = %ld, total size = %ld \n", sizeof(struct s3), n_stops * n_rounds * sizeof(struct s3));
    printf ("s4 size = %ld, total size = %ld \n", sizeof(struct s4), n_stops * n_rounds * sizeof(struct s4));
    printf ("t0 size = %ld, total size = %ld \n", sizeof(struct t0), n_stops * n_rounds * sizeof(struct t0));
    printf ("t1 size = %ld, total size = %ld \n", sizeof(struct t1), n_stops * n_rounds * sizeof(struct t1));
    printf ("t1 colstore,   total size = %ld \n", (sizeof(uint8_t) + sizeof(uint32_t)) * n_stops * n_rounds);
    return 0;
}

