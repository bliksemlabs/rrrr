#include "rrrr.h"

struct context ctx;

void die(const char *msg) {
    syslog(LOG_ERR, "%s", msg);
    exit(EXIT_FAILURE);
}

void setup() {

    #define INPUT_FILE "/tmp/timetable.dat"

    /* Logging */
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    syslog(LOG_INFO, "starting up");

    /* Map input file*/
    int fd = open(INPUT_FILE, O_RDONLY);
    if (fd == -1) 
        die("could not find input file");

    struct stat st;
    if (stat(INPUT_FILE, &st) == -1) 
        die("could not stat input file");
    
    ctx.data_size = st.st_size;
    ctx.data = mmap((caddr_t)0, ctx.data_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ctx.data == (caddr_t)(-1)) 
        die("could not map input file");

    void *d = ctx.data;
    timetable_header_t *header = (timetable_header_t*) d;
    if( strncmp("TTABLEV1", header->version_string, 8) )
        die("the input file does not appear to be a timetable");
    ctx.nstops = header->nstops;
    ctx.nroutes = header->nroutes;
    ctx.stop_coords = (stop_coord_t*) (d + 8 + 8 * sizeof(int)); // position 40
    ctx.stops = (stop_t*) (d + header->loc_stops);
    ctx.routes = (route_t*) (d + header->loc_routes);
    ctx.route_stops = (int*) (d + header->loc_route_stops);
    ctx.stop_times = (int*) (d + header->loc_stop_times);
    ctx.stop_routes = (int*) (d + header->loc_stop_routes);
    ctx.transfers = (xfer_t*) (d + header->loc_transfers);
    
}

void html_header() {
    printf("Content-type: text/plain\r\n\r\nBEGIN OUTPUT\n");
}

void dump_request(request_t *req) {
    printf("from: %d\n"
           "to: %d\n"
           "time: %d\n"
           "speed: %f\n", req->from, req->to, req->time, req->walk_speed);
    printf("\nCONTEXT\n"
           "nstops: %d\n"
           "nroutes: %d\n", ctx.nstops, ctx.nroutes);
    printf("\nSTOPS\n");
    int i;
    for (i=0; i<ctx.nstops; i++) {
        printf("stop %d at lat %f lon %f\n", i, ctx.stop_coords[i].lat, ctx.stop_coords[i].lon);
        stop_t s0 = ctx.stops[i];
        stop_t s1 = ctx.stops[i+1];
        int j0 = s0.stop_routes_offset;
        int j1 = s1.stop_routes_offset;
        int j;
        printf("served by routes ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", ctx.stop_routes[j]);
        }
        printf("\n");
    }
    printf("\nROUTES\n");
    for (i=0; i<ctx.nroutes; i++) {
        printf("route %d\n", i);
        route_t r0 = ctx.routes[i];
        route_t r1 = ctx.routes[i+1];
        int j0 = r0.route_stops_offset;
        int j1 = r1.route_stops_offset;
        int j;
        printf("serves stops ");
        for (j=j0; j<j1; ++j) {
            printf("%d ", ctx.route_stops[j]);
        }
        printf("\n");
    }
}

void mainloop() {
    int (*arrival)[ctx.nstops]; // arrival times [round][stop]
    int (*back_trip)[ctx.nstops];
    int (*back_stop)[ctx.nstops];
    request_t req;
    while(FCGI_Accept() >= 0) {
        html_header();
        parse_query_params(&req);
        dump_request(&req);
    }
}

void teardown() {
    munmap(ctx.data, ctx.data_size);
}

int main(int argc, char **argv) {
    setup();
    mainloop();
    teardown();
    return EXIT_SUCCESS;
}
