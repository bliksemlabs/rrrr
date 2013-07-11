/* client.c : test client */

#include <time.h>
#include <zmq.h>
#include <czmq.h>
#include <assert.h>
#include <unistd.h>
#include "rrrr.h"
#include "router.h"
#include "config.h"

static bool verbose = false;
static bool randomize = false;
static uint32_t from_s = 0;
static uint32_t to_s = 1;

static void client_task (void *args, zctx_t *ctx, void *pipe) {
    uint32_t n_requests = *((uint32_t *) args);
    syslog (LOG_INFO, "test client thread will send %d requests", n_requests);
    // connect client thread to load balancer
    void *sock = zsocket_new (ctx, ZMQ_REQ); // auto-deleted with thread
    uint32_t rc = zsocket_connect (sock, CLIENT_ENDPOINT);
    assert (rc == 0);
    uint32_t request_count = 0;
    while (true) {
        router_request_t req;
        router_request_initialize (&req);
        if (randomize) 
            router_request_randomize (&req);
        else {
            req.from=from_s;
            req.to=to_s;
        }
        // if (verbose) router_request_dump(&router, &req); // router is not available in client
        zmq_send(sock, &req, sizeof(req), 0);
        // syslog (LOG_INFO, "test client thread has sent %d requests\n", request_count);
        char *reply = zstr_recv (sock);
        if (!reply) 
            break;
        if (verbose) 
            printf ("%s", reply);
        free (reply);
        if (++request_count >= n_requests)
            break;
    }
    syslog (LOG_INFO, "test client thread terminating");
    zstr_send (pipe, "done");
}

void usage() {
    printf("usage: 'client rand [nreqs] [nthreads]' or 'client id [from_stop_id] [to_stop_id]'\n" );
    exit (1);
}

int main (int argc, char **argv) {
    
    // initialize logging
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog (PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    syslog (LOG_INFO, "test client starting");

    // ensure different random requests on different runs
    srand(time(NULL)); 
    
    // read and range-check parameters
    uint32_t n_requests = 1;
    uint32_t concurrency = RRRR_TEST_CONCURRENCY;
    if (argc != 4)
        usage();
    
    if (strcmp(argv[1], "rand") == 0) {
        randomize = true;
    } else if (strcmp(argv[1], "id") == 0) {
        randomize = false;
    } else {
        usage();
    }

    if (randomize && argc > 2) {
        n_requests = atoi(argv[2]);
        if (n_requests < 0)
            n_requests = 1;
        if (argc > 3)
            concurrency = atoi(argv[3]);
    } else {
        from_s = atoi(argv[2]);
        to_s = atoi(argv[3]);
        n_requests = 1;
        concurrency = 1;
    }
    if (concurrency < 1 || concurrency > 32)
        concurrency = RRRR_TEST_CONCURRENCY;
    if (concurrency > n_requests)
        concurrency = n_requests;

    syslog (LOG_INFO, "test client number of requests: %d", n_requests);
    syslog (LOG_INFO, "test client concurrency: %d", concurrency);
    verbose = (n_requests == 1);
    
    // divide up work between threads
    uint32_t n_reqs[concurrency];
    for (uint32_t i = 0; i < concurrency; i++)
        n_reqs[i] = n_requests / concurrency;
    for (uint32_t i = 0; i < n_requests % concurrency; i++)
        n_reqs[i] += 1;

    // track runtime
    struct timeval t0, t1;
    
    // create threads
    void *pipes[concurrency];
    zctx_t *ctx = zctx_new ();
    gettimeofday(&t0, NULL);
    for (uint32_t n = 0; n < concurrency; n++)
        pipes[n] = zthread_fork (ctx, client_task, n_reqs + n);

    // wait for all threads to complete
    for (uint32_t n = 0; n < concurrency; n++) {
        char *s = zstr_recv (pipes[n]);
        if (s == NULL) break; // interrupted
        free (s);
    }

    // output summary information
    gettimeofday(&t1, NULL);
    double dt = ((t1.tv_usec + 1000000 * t1.tv_sec) - (t0.tv_usec + 1000000 * t0.tv_sec)) / 1000000.0;
    syslog (LOG_INFO, "%d threads, %d total requests, %f sec total time (%f req/sec)",
        concurrency, n_requests, dt, n_requests / dt);
    
    //printf("%c", '\0x1A');
    
    // teardown
    zctx_destroy (&ctx);
    closelog();
    
}

