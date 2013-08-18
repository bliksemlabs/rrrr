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

void main () {
    int n_stops = 50000;
    int n_rounds = 8;
    printf ("size = %ld total size = %ld \n", sizeof(struct s0), n_stops * n_rounds * sizeof(struct s0));
    printf ("size = %ld total size = %ld \n", sizeof(struct s1), n_stops * n_rounds * sizeof(struct s1));
    printf ("size = %ld total size = %ld \n", sizeof(struct s2), n_stops * n_rounds * sizeof(struct s2));
    printf ("size = %ld total size = %ld \n", sizeof(struct s3), n_stops * n_rounds * sizeof(struct s3));
}

