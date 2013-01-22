#include <syslog.h>
#include <fcgi_stdio.h>
#include <stdlib.h>
#include "config.h"
#include "transitdata.h"
#include "router.h"

int main(int argc, char **argv) {

    /* SETUP */
    
    // logging
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(CONFIG_PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    syslog(LOG_INFO, "starting up");
    
    // load transit data from disk
    transit_data_t tdata;
    transit_data_load(CONFIG_INPUT_FILE, &tdata);
    
    // initialize router
    router_t router;
    router_setup(&router, &tdata);

    /* MAIN LOOP */
    router_request_t req;
    while(FCGI_Accept() >= 0) {
        printf("Content-type: text/plain\r\n\r\n");
        router_request_from_qstring(&req);
        router_request_dump(&req);
        transit_data_dump(&tdata);
        //router_route(&req);
        //router_result(&req);
    }

    /* TEAR DOWN */
    router_teardown(&router);
    transit_data_close(&tdata);

    return EXIT_SUCCESS;
}

