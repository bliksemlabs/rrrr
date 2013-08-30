/* tdata.c : handles memory mapped data file containing transit timetable etc. */

#define _GNU_SOURCE

#include "tdata.h" // make sure it works alone
#include "config.h"
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

// file-visible struct
typedef struct tdata_header tdata_header_t;
struct tdata_header {
    char version_string[8]; // should read "TTABLEV1"
    uint64_t calendar_start_time;
    uint32_t n_stops;
    uint32_t n_routes;
    uint32_t loc_stops;
    uint32_t loc_stop_coords;
    uint32_t loc_routes;
    uint32_t loc_route_stops;
    uint32_t loc_stop_times;
    uint32_t loc_trips;
    uint32_t loc_stop_routes;
    uint32_t loc_transfer_target_stops; 
    uint32_t loc_transfer_dist_meters; 
    uint32_t loc_stop_ids; 
    uint32_t loc_route_ids; 
    uint32_t loc_trip_ids; 
    uint32_t loc_trip_active; 
    uint32_t loc_route_active; 
};

inline char *tdata_stop_id_for_index(tdata_t *td, uint32_t stop_index) {
    return td->stop_ids + (td->stop_id_width * stop_index);
}

// TODO rename this, confusing.
inline uint32_t tdata_stop_name_for_index(tdata_t *td, char* stop_name, uint32_t start_index) {
    for (uint32_t stop_index = start_index; stop_index < td->n_stops; stop_index++) {
        if (strcasestr(td->stop_ids + (td->stop_id_width * stop_index), stop_name)) {
            return stop_index;
        }
    }
    return NONE;
}

inline char *tdata_route_id_for_index(tdata_t *td, uint32_t route_index) {
    return td->route_ids + (td->route_id_width * route_index);
}

inline char *tdata_trip_ids_for_route(tdata_t *td, uint32_t route_index) {
    route_t route = (td->routes)[route_index];
    uint32_t char_offset = route.trip_ids_offset * td->trip_id_width;
    return td->trip_ids + char_offset;
}

inline uint32_t *tdata_trip_masks_for_route(tdata_t *td, uint32_t route_index) {
    route_t route = (td->routes)[route_index];
    return td->trip_active + route.trip_ids_offset;
}

void tdata_check_coherent (tdata_t *td) {
    /* Check that all lat/lon look like valid coordinates for this part of Europe (including Paris, Berlin etc. */
    float min_lat = 42.0;
    float max_lat = 54.0;
    float min_lon = -1.0;
    float max_lon = 15.0;
    for (int s = 0; s < td->n_stops; ++s) {
        latlon_t ll = td->stop_coords[s];
        if (ll.lat < min_lat || ll.lat > max_lat || ll.lon < min_lon || ll.lon > max_lon) {
            printf ("stop lat/lon out of range: lat=%f, lon=%f \n", ll.lat, ll.lon);
        }
    }
    /* Check that all timedemand types start at 0 and consist of monotonically increasing times. */
    for (int r = 0; r < td->n_routes; ++r) {
        route_t route = td->routes[r];
        trip_t *trips = td->trips + route.trip_ids_offset;
        int n_nonincreasing_trips = 0;
        for (int t = 0; t < route.n_trips; ++t) {
            trip_t trip = trips[t];
            stoptime_t *prev_st = NULL;
            for (int s = 0; s < route.n_stops; ++s) {
                stoptime_t *st = td->stop_times + trip.stop_times_offset + s;
                if (s == 0 && st->arrival != 0) printf ("timedemand type begins at %d,%d not 0.\n", st->arrival, st->departure);
                if (st->departure < st->arrival) printf ("departure before arrival at route %d, trip %d, stop %d.\n", r, t, s);
                if (prev_st != NULL) {
                    if (st->arrival < prev_st->departure) {
                        // printf ("negative travel time arriving at route %d, trip %d, stop %d.\n", r, t, s);
                        // printf ("(%d, %d) -> (%d, %d)\n", prev_st->arrival, prev_st->departure, st->arrival, st->departure);
                        n_nonincreasing_trips += 1;
                    } // there are also lots of 0 travel times...	
                }
                prev_st = st;                
            }
        }
        if (n_nonincreasing_trips > 0) printf ("route %d has %d trips with negative travel times\n", r, n_nonincreasing_trips);
    }

}

/* Map an input file into memory and reconstruct pointers to its contents. */
void tdata_load(char *filename, tdata_t *td) {

    uint32_t fd = open(filename, O_RDWR);
    if (fd == -1) 
        die("could not find input file");

    struct stat st;
    if (stat(filename, &st) == -1) 
        die("could not stat input file");
    
    td->base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    td->size = st.st_size;
    if (td->base == (void*)(-1)) 
        die("could not map input file");

    void *b = td->base;
    tdata_header_t *header = b;
    if( strncmp("TTABLEV1", header->version_string, 8) )
        die("the input file does not appear to be a timetable");
    td->calendar_start_time = header->calendar_start_time;
    td->n_stops = header->n_stops;
    td->n_routes = header->n_routes;
    td->stops = (stop_t*) (b + header->loc_stops);
    td->stop_coords = (latlon_t*) (b + header->loc_stop_coords);
    td->routes = (route_t*) (b + header->loc_routes);
    td->route_stops = (uint32_t *) (b + header->loc_route_stops);
    td->stop_times = (stoptime_t*) (b + header->loc_stop_times);
    td->trips = (trip_t*) (b + header->loc_trips);
    td->stop_routes = (uint32_t *) (b + header->loc_stop_routes);
    td->transfer_target_stops = (uint32_t *) (b + header->loc_transfer_target_stops);
    td->transfer_dist_meters = (uint8_t *) (b + header->loc_transfer_dist_meters);
    //maybe replace with pointers because there's a lot of wasted space?
    td->stop_id_width = *((uint32_t *) (b + header->loc_stop_ids));
    td->stop_ids = (char*) (b + header->loc_stop_ids + sizeof(uint32_t));
    td->route_id_width = *((uint32_t *) (b + header->loc_route_ids));
    td->route_ids = (char*) (b + header->loc_route_ids + sizeof(uint32_t));
    td->trip_id_width = *((uint32_t *) (b + header->loc_trip_ids));
    td->trip_ids = (char*) (b + header->loc_trip_ids + sizeof(uint32_t));
    td->trip_active = (uint32_t*) (b + header->loc_trip_active);
    td->route_active = (uint32_t*) (b + header->loc_route_active);

    tdata_check_coherent(td);
    D tdata_dump(td);
}

void tdata_close(tdata_t *td) {
    munmap(td->base, td->size);
}

// TODO should pass pointer to tdata?
inline uint32_t *tdata_stops_for_route(tdata_t td, uint32_t route) {
    route_t route0 = td.routes[route];
    return td.route_stops + route0.route_stops_offset;
}

inline uint32_t tdata_routes_for_stop(tdata_t *td, uint32_t stop, uint32_t **routes_ret) {
    stop_t stop0 = td->stops[stop];
    stop_t stop1 = td->stops[stop + 1];
    *routes_ret = td->stop_routes + stop0.stop_routes_offset;
    return stop1.stop_routes_offset - stop0.stop_routes_offset;
}

// TODO used only in dumping routes; trip_index is not used in the expression?
inline stoptime_t *tdata_timedemand_type(tdata_t *td, uint32_t route_index, uint32_t trip_index) {
    return td->stop_times + td->trips[td->routes[route_index].trip_ids_offset].stop_times_offset;
}

inline trip_t *tdata_trips_for_route(tdata_t *td, uint32_t route_index) {
    return td->trips + td->routes[route_index].trip_ids_offset;
}

void tdata_dump_route(tdata_t *td, uint32_t route_idx, uint32_t trip_idx) {
    uint32_t *stops = tdata_stops_for_route(*td, route_idx);
    route_t route = td->routes[route_idx];
    printf("\nRoute details for '%s' [%d] (n_stops %d, n_trips %d)\n", 
        tdata_route_id_for_index(td, route_idx), route_idx, route.n_stops, route.n_trips);
    printf("stop sequence, stop name (index), departures  \n");
    for (uint32_t ti = 0; ti < route.n_trips; ++ti) {
        // TODO should this really be a 2D array ?
        stoptime_t (*times)[route.n_stops] = (void*) tdata_timedemand_type(td, route_idx, ti);
        for (uint32_t si = 0; si < route.n_stops; ++si) {
            char *stop_id = tdata_stop_id_for_index (td, stops[si]);
            printf("%4d %35s [%06d] : %s", si, stop_id, stops[si], timetext(times[ti][si].departure + td->trips[ti].begin_time));
         }
         printf("\n");
    }
    printf("\n");
}

void tdata_dump(tdata_t *td) {
    printf("\nCONTEXT\n"
           "n_stops: %d\n"
           "n_routes: %d\n", td->n_stops, td->n_routes);
    printf("\nSTOPS\n");
    for (uint32_t i = 0; i < td->n_stops; i++) {
        printf("stop %d at lat %f lon %f\n", i, td->stop_coords[i].lat, td->stop_coords[i].lon);
        stop_t s0 = td->stops[i];
        stop_t s1 = td->stops[i+1];
        uint32_t j0 = s0.stop_routes_offset;
        uint32_t j1 = s1.stop_routes_offset;
        uint32_t j;
        printf("served by routes ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", td->stop_routes[j]);
        }
        printf("\n");
    }
    printf("\nROUTES\n");
    for (uint32_t i = 0; i < td->n_routes; i++) {
        printf("route %d\n", i);
        printf("having trips %d\n", td->routes[i].n_trips);
        route_t r0 = td->routes[i];
        route_t r1 = td->routes[i+1];
        uint32_t j0 = r0.route_stops_offset;
        uint32_t j1 = r1.route_stops_offset;
        uint32_t j;
        printf("serves stops ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", td->route_stops[j]);
        }
        printf("\n");
    }
    printf("\nSTOPIDS\n");
    for (uint32_t i = 0; i < td->n_stops; i++) {
        printf("stop %03d has id %s \n", i, tdata_stop_id_for_index(td, i));
    }
#if 0
    printf("\nROUTEIDS, TRIPIDS\n");
    for (uint32_t i = 0; i < td->n_routes; i++) {
        printf("route %03d has id %s and first trip id %s \n", i, 
            tdata_route_id_for_index(td, i),
            tdata_trip_ids_for_route(td, i));
    }
#endif
}


// tdata_get_route_stops

/* optional stop ids, names, coordinates... */
