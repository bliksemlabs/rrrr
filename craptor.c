#include "craptor.h"

struct context ctx;

void die(const char *msg) {
    syslog(LOG_ERR, "%s", msg);
    exit(EXIT_FAILURE);
}

void setup() {

    #define INPUT_FILE "/home/abyrd/timetable.dat"

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

    int *d = (int*) ctx.data;
    ctx.nstops = *d++;
    ctx.nroutes = *d++;
    ctx.stops = (stop_t*) d;
    d = (int*) (ctx.stops + ctx.nstops);
    ctx.route_stops_offsets = d;
    d += ctx.nroutes;
    ctx.stop_times_offsets = d;
    d += ctx.nroutes;
    
}

void html_header() {
    printf("Content-type: text/plain\r\n\r\nBEGIN OUTPUT\n");
}

void dump_request(request_t *req) {
    printf("from: %d\n"
           "to: %d\n"
           "time: %d\n"
           "speed: %f\n", req->from, req->to, req->time, req->walk_speed);
    printf("context:\n"
           "nstops: %d\n"
           "nroutes: %d\n", ctx.nstops, ctx.nroutes);
    int i;
    for (i=0; i<ctx.nstops; i++) {
        printf("%d %f %f\n", i, ctx.stops[i].lat, ctx.stops[i].lon);
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
