/* transitdata.c : handles memory mapped data file with timetable etc. */
#include "transitdata.h" // make sure it works alone
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

// file-visible struct
typedef struct transit_data_header transit_data_header_t;
struct transit_data_header {
    char version_string[8]; // should read "TTABLEV1"
    int nstops;
    int nroutes;
    int loc_stops;
    int loc_routes;
    int loc_route_stops;
    int loc_stop_times;
    int loc_stop_routes;
    int loc_transfers; 
    int loc_stop_ids; 
};

inline char *transit_data_stop_id_for_index(transit_data_t *td, int index) {
    return td->stop_ids + (td->stop_id_width * index);
}

/* Map an input file into memory and reconstruct pointers to its contents. */
void transit_data_load(char *filename, transit_data_t *td) {

    int fd = open(filename, O_RDONLY);
    if (fd == -1) 
        die("could not find input file");

    struct stat st;
    if (stat(filename, &st) == -1) 
        die("could not stat input file");
    
    td->base = mmap((void*)0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    td->size = st.st_size;
    if (td->base == (void*)(-1)) 
        die("could not map input file");

    void *b = td->base;
    transit_data_header_t *header = b;
    if( strncmp("TTABLEV1", header->version_string, 8) )
        die("the input file does not appear to be a timetable");
    td->nstops = header->nstops;
    td->nroutes = header->nroutes;
    td->stop_coords = (coord_t*) (b + 8 + 8 * sizeof(int)); // position 40
    td->stops = (stop_t*) (b + header->loc_stops);
    td->routes = (route_t*) (b + header->loc_routes);
    td->route_stops = (int*) (b + header->loc_route_stops);
    td->stop_times = (int*) (b + header->loc_stop_times);
    td->stop_routes = (int*) (b + header->loc_stop_routes);
    td->transfers = (transfer_t*) (b + header->loc_transfers);
    td->stop_id_width = *((int*) (b + header->loc_stop_ids));
    td->stop_ids = (char*) (b + header->loc_stop_ids + sizeof(int));
}

void transit_data_close(transit_data_t *td) {
    munmap(td->base, td->size);
}

// change to return number of stops, store pointer to array.
inline int *transit_data_stops_for_route(transit_data_t td, int route, int **next_route) {
    route_t route0 = td.routes[route];
    route_t route1 = td.routes[route + 1];
    *next_route = td.route_stops + route1.route_stops_offset;
    return td.route_stops + route0.route_stops_offset;
}

inline int *transit_data_stoptimes_for_route(transit_data_t td, int route, int **next_route) {
    route_t route0 = td.routes[route];
    route_t route1 = td.routes[route + 1];
    *next_route = td.stop_times + route1.stop_times_offset;
    return td.stop_times + route0.stop_times_offset;
}

void transit_data_dump_route(transit_data_t *td, int route) {
    int *s_end, *t_end;
    int *s = transit_data_stops_for_route(*td, route, &s_end);
    int *t = transit_data_stoptimes_for_route(*td, route, &t_end);
    int nstops = s_end - s;
    printf("ROUTE %d NSTOPS %d\n", route, nstops);
    printf("SSEQ SINDEX  \n");
    for (int i = 0; i < nstops; ++i) {
        printf("%4d %6d : ", i, s[i]);
        for (int *t2 = &(t[i]); t2 < t_end; t2 += nstops) {
            printf("%s ", timetext(*t2));
        }
        printf("\n");
    }
}

void transit_data_dump(transit_data_t *td) {
    printf("\nCONTEXT\n"
           "nstops: %d\n"
           "nroutes: %d\n", td->nstops, td->nroutes);
    printf("\nSTOPS\n");
    for (int i = 0; i < td->nstops; i++) {
        printf("stop %d at lat %f lon %f\n", i, td->stop_coords[i].lat, td->stop_coords[i].lon);
        stop_t s0 = td->stops[i];
        stop_t s1 = td->stops[i+1];
        int j0 = s0.stop_routes_offset;
        int j1 = s1.stop_routes_offset;
        int j;
        printf("served by routes ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", td->stop_routes[j]);
        }
        printf("\n");
    }
    printf("\nROUTES\n");
    for (int i = 0; i < td->nroutes; i++) {
        printf("route %d\n", i);
        route_t r0 = td->routes[i];
        route_t r1 = td->routes[i+1];
        int j0 = r0.route_stops_offset;
        int j1 = r1.route_stops_offset;
        int j;
        printf("serves stops ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", td->route_stops[j]);
        }
        printf("\n");
    }
    printf("\nSTOPIDS\n");
    int stride = td->stop_id_width;
    for (int i = 0; i < td->nstops; i++) {
        printf("stop %03d has id %s \n", i, td->stop_ids + (i * stride));
    }
}


// transit_data_get_route_stops

/* optional stop ids, names, coordinates... */
