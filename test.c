/* test.c : test client */

#include <time.h>
#include <zmq.h>
#include <czmq.h>
#include <assert.h>
#include <unistd.h>
#include "rrrr.h"
#include "router.h"
#include "config.h"

static void client_task (void *args, zctx_t *ctx, void *pipe) {

    // connect client thread to load balancer
    void *sock = zsocket_new (ctx, ZMQ_REQ); // auto-deleted with thread
    // int rc = zsocket_connect (sock, "ipc://frontend.ipc"); 
    int rc = zsocket_connect (sock, CLIENT_ENDPOINT);
    assert (rc == 0);
    int request_count = 0;
    while (true) {
        //sleep (1);
        router_request_t req;
        router_request_randomize(&req);
        //router_request_dump(&req);
        zmq_send(sock, &req, sizeof(req), 0);
        //syslog (LOG_INFO, "test client sent %d requests\n", request_count);
        char *reply = zstr_recv (sock);
        if (!reply)
            break;
        //syslog (LOG_INFO, "test client received response: %s\n", reply);
        free (reply);
        if (++request_count == RRRR_TEST_REQUESTS)
            break;
    }
    syslog (LOG_INFO, "test client thread terminating");
    zstr_send (pipe, "done");

}

int main (void) {
    
    // initialize logging
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog (PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    syslog (LOG_INFO, "test client starting");

    struct timeval t0, t1;
    
    // create threads
    void *pipes[RRRR_TEST_CONCURRENCY];
    zctx_t *ctx = zctx_new ();
    gettimeofday(&t0, NULL);
    for (int n = 0; n < RRRR_TEST_CONCURRENCY; n++)
        pipes[n] = zthread_fork (ctx, client_task, NULL);

    // wait for all threads to complete
    for (int n = 0; n < RRRR_TEST_CONCURRENCY; n++) {
        char *s = zstr_recv (pipes[n]);
        if (s == NULL) break; // interrupted
        free (s);
    }

    // output summary information
    gettimeofday(&t1, NULL);
    double dt = ((t1.tv_usec + 1000000 * t1.tv_sec) - (t0.tv_usec + 1000000 * t0.tv_sec)) / 1000000.0;
    int total_requests = RRRR_TEST_CONCURRENCY * RRRR_TEST_REQUESTS;
    syslog (LOG_INFO, "%d threads, %d requests each, %d total requests, %f sec total time (%f req/sec)",
        RRRR_TEST_CONCURRENCY, RRRR_TEST_REQUESTS, total_requests, dt, total_requests / dt);

    // teardown
    zctx_destroy (&ctx);
    closelog();
    
}

