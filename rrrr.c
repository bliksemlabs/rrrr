#include <syslog.h>
#include <stdlib.h>
#include <zmq.h>
#include <czmq.h>
#include <assert.h>
#include "config.h"
#include "rrrr.h"
#include "transitdata.h"
#include "router.h"

int main(int argc, char **argv) {

    /* SETUP */
    
    // logging
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    syslog(LOG_INFO, "worker starting up");
    
    // load transit data from disk
    transit_data_t tdata;
    transit_data_load(RRRR_INPUT_FILE, &tdata);
    
    // initialize router
    router_t router;
    router_setup(&router, &tdata);

    // establish zmq connection
    zctx_t *zctx = zctx_new ();
    void *zsock = zsocket_new(zctx, ZMQ_REQ);
    int zrc = zsocket_connect(zsock, "tcp://localhost:9293");
    if (zrc != 0) exit(1);
    
    // signal to the broker/load balancer that this worker is ready
    // move to shared header file
    zframe_t *frame = zframe_new (WORKER_READY, 1);
    zframe_send (&frame, zsock, 0);
    syslog(LOG_INFO, "worker sent ready message to load balancer");

    /* MAIN LOOP */
    int request_count = 0;
    while (true) {
        zmsg_t *msg = zmsg_recv (zsock);
        if (!msg) // interrupted (signal)
            break; 
        if (++request_count % 100 == 0)
            syslog(LOG_INFO, "worker received %d requests\n", request_count);

        // only manipulate the last frame, then send the recycled message back to the broker
        zframe_t *frame = zmsg_last (msg);
        if (zframe_size (frame) == sizeof (router_request_t)) {
            router_request_t *req;
            req = (router_request_t*) zframe_data (frame);
            //syslog(LOG_INFO, "received request");
            //router_request_from_qstring(&req);
            //router_request_dump(&req);
            //transit_data_dump(&tdata);
            router_route(&router, req);
            //router_result_dump(&router, &req);
            zframe_reset (frame, "OK", 2);
        } else {
            syslog(LOG_WARNING, "worker received reqeust with wrong length");
            zframe_reset (frame, "ERR", 3);
        }

        // send response to broker, thereby requesting more work
        zmsg_send (&msg, zsock);
    }
    
    /* TEAR DOWN */
    syslog(LOG_INFO, "worker terminating");
    // frame = zframe_new (WORKER_LEAVING, 1);
    // zframe_send (&frame, zmq_sock, 0);
    // syslog(LOG_INFO, "departure message sent to load balancer");
    // zmsg_t *msg = zmsg_recv (zmq_sock);
    transit_data_close(&tdata);
    zctx_destroy (&zctx); //zmq_close(socket) necessary before context destroy?
    exit(EXIT_SUCCESS);
}

