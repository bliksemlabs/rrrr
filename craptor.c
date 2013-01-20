#include "craptor.h"

struct context {
    void *data;
    size_t data_size;
};

/* global */ struct context ctx;

void die(const char *msg) {
    syslog(LOG_ERR, "%s", msg);
    exit(EXIT_FAILURE);
}

void setup() {

    #define INPUT_FILE "timetable.dat"
    
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
    
}

void html_header() {
    printf("Content-type: text/plain\r\n\r\nBEGIN OUTPUT\n");
}

void dump_request(request_t *req) {
    printf("from: %d\nto: %d\ntime: %d\n", req->from, req->to, req->time);
}

void mainloop() {
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
