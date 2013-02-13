/* Load-balancing broker using CZMQ API. Borrows heavily from load balancer pattern in 0MQ Guide. */

#include <syslog.h>
#include <czmq.h>
#include "rrrr.h"
#include "config.h"

int main (void) {

    // initialize logging
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    syslog(LOG_INFO, "broker starting up");

    zctx_t *ctx = zctx_new ();
    void *frontend = zsocket_new (ctx, ZMQ_ROUTER);
    void *backend = zsocket_new (ctx, ZMQ_ROUTER);
    zsocket_bind (frontend, CLIENT_ENDPOINT);
    zsocket_bind (backend, WORKER_ENDPOINT);
    int frx = 0, ftx = 0, brx = 0, btx = 0, nworkers = 0, npoll = 0;
    
    //  Queue of available workers
    zlist_t *workers = zlist_new ();

    while (true) {
        if (++npoll % 100 == 0)
            syslog(LOG_INFO, "broker: frx %04d ftx %04d brx %04d btx %04d / %d workers\n", 
                frx, ftx, brx, btx, nworkers);
        zmq_pollitem_t items [] = {
            { backend,  0, ZMQ_POLLIN, 0 },
            { frontend, 0, ZMQ_POLLIN, 0 }
        };
        //  Poll frontend only if we have available workers
        int rc = zmq_poll (items, zlist_size (workers)? 2: 1, -1);
        if (rc == -1)
            break;              //  Interrupted
        //  Handle worker activity on backend
        if (items [0].revents & ZMQ_POLLIN) {
            //  Use worker identity for load-balancing
            zmsg_t *msg = zmsg_recv (backend);
            if (!msg)
                break;          //  Interrupted
            zframe_t *identity = zmsg_unwrap (msg);
            zlist_append (workers, identity);

            //  Forward message to client if it's not a READY
            zframe_t *frame = zmsg_first (msg);
            if (memcmp (zframe_data (frame), WORKER_READY, 1) == 0) {
                zmsg_destroy (&msg);
                nworkers++;
            } else {
                brx++;
                zmsg_send (&msg, frontend);
                ftx++;
            }
        }
        if (items [1].revents & ZMQ_POLLIN) {
            //  Get client request, route to first available worker
            zmsg_t *msg = zmsg_recv (frontend);
            frx++;
            if (msg) {
                zmsg_wrap (msg, (zframe_t *) zlist_pop (workers));
                zmsg_send (&msg, backend);
                btx++;
            }
        }
    }
    
    //  When we're done, clean up properly
    syslog(LOG_INFO, "broker terminating");
    while (zlist_size (workers)) {
        zframe_t *frame = (zframe_t *) zlist_pop (workers);
        zframe_destroy (&frame);
    }
    zlist_destroy (&workers);
    zctx_destroy (&ctx);
    return 0;

}
