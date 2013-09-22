/* worker.c : worker processes that handle routing requests */

#include <syslog.h>
#include <stdlib.h>
#include <zmq.h>
#include <czmq.h>
#include <assert.h>
#include "config.h"
#include "rrrr.h"
#include "tdata.h"
#include "router.h"
#include "parse.h"
#include "json.h"

int main(int argc, char **argv) {

    /* SETUP */
    
    // logging
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    syslog(LOG_INFO, "worker starting up");
    
    // load transit data from disk
    tdata_t tdata;
    tdata_load(RRRR_INPUT_FILE, &tdata);
    
    // initialize router
    router_t router;
    router_setup(&router, &tdata);
    //tdata_dump(&tdata); // debug timetable file format
    
    // establish zmq connection
    zctx_t *zctx = zctx_new ();
    void *zsock = zsocket_new(zctx, ZMQ_REQ);
    uint32_t zrc = zsocket_connect(zsock, WORKER_ENDPOINT);
    if (zrc != 0) exit(1);
    
    // signal to the broker/load balancer that this worker is ready
    zframe_t *frame = zframe_new (WORKER_READY, 1);
    zframe_send (&frame, zsock, 0);
    syslog(LOG_INFO, "worker sent ready message to load balancer");

    /* MAIN LOOP */
    uint32_t request_count = 0;
    char result_buf[8000];
    while (true) {
        zmsg_t *msg = zmsg_recv (zsock);
        if (!msg) // interrupted (signal)
            break; 
        if (++request_count % 100 == 0)
            syslog(LOG_INFO, "worker received %d requests\n", request_count);
        // only manipulate the last frame, then send the recycled message back to the broker
        zframe_t *frame = zmsg_last (msg);
        char *qstring = (char *) zframe_data (frame);
        printf("%s\n", qstring);
        router_request_t preq;
        parse_request_from_qstring(&preq, &tdata, qstring);
        router_request_t req;
        memcpy(&req, &preq, sizeof(preq));
        D printf ("Searching with request: \n");
        // I router_request_dump (&router, &req);
        router_request_dump (&router, &req);
        router_route (&router, &req);
        // repeat search in reverse to compact transfers
        uint32_t n_reversals = req.arrive_by ? 1 : 2;
        //n_reversals = 0; // DEBUG turn off reversals
        for (uint32_t i = 0; i < n_reversals; ++i) {
            router_request_reverse (&router, &req); // handle case where route is not reversed
            D printf ("Repeating search with reversed request: \n");
            D router_request_dump (&router, &req);
            router_route (&router, &req);
        }
        uint32_t result_length = json_result_dump(&router, &preq, result_buf, 8000);
        
        zframe_reset (frame, result_buf, result_length);
        // send response to broker, thereby requesting more work
        zmsg_send (&msg, zsock);
    }
    
    /* TEAR DOWN */
    syslog(LOG_INFO, "worker terminating");
    // frame = zframe_new (WORKER_LEAVING, 1);
    // zframe_send (&frame, zmq_sock, 0);
    // syslog(LOG_INFO, "departure message sent to load balancer");
    // zmsg_t *msg = zmsg_recv (zmq_sock);
    tdata_close(&tdata);
    zctx_destroy (&zctx); //zmq_close(socket) necessary before context destroy?
    exit(EXIT_SUCCESS);
}

