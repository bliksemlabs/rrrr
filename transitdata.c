/* transit_data.c : handles memory mapped data file with timetable etc. */
#include "transitdata.h" // make sure it works alone
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <fcgi_stdio.h>

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
};

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

}

void transit_data_close(transit_data_t *td) {
    munmap(td->base, td->size);
}

void transit_data_dump(transit_data_t *td) {
    printf("\nCONTEXT\n"
           "nstops: %d\n"
           "nroutes: %d\n", td->nstops, td->nroutes);
    printf("\nSTOPS\n");
    int i;
    for (i=0; i<td->nstops; i++) {
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
    for (i=0; i<td->nroutes; i++) {
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
}


// transit_data_get_route_stops

/* optional stop ids, names, coordinates... */
