/* router.c : the main routing algorithm */
#include "router.h"
#include "util.h"
#include "config.h"
#include "qstring.h"
#include <stdlib.h>
#include <fcgi_stdio.h>
#include <string.h>

#define BUFLEN 255

void router_setup(router_t *router, transit_data_t *td) {
    int table_size = td->nstops * CONFIG_MAX_ROUNDS;
    router->arrivals = malloc(sizeof(int) * table_size);
    if (router->arrivals == NULL)
        die("failed to allocate scratch space");
}

void router_teardown(router_t *router) {
    free(router->arrivals);
}

bool router_request_from_qstring(router_request_t *req) {
    char *qstring = getenv("QUERY_STRING");
    if (qstring == NULL)
        return false;
    char key[BUFLEN];
    char *val;
    // set defaults
    req->walk_speed = 1.3; // m/sec
    req->from = req->to = req->time = -1; 
    req->arrive_by = false;
    while (qstring_next_pair(qstring, key, &val, BUFLEN)) {
        if (strcmp(key, "time") == 0) {
            req->time = atoi(val);
        } else if (strcmp(key, "from") == 0) {
            req->from = atoi(val);
        } else if (strcmp(key, "to") == 0) {
            req->to = atoi(val);
        } else if (strcmp(key, "speed") == 0) {
            req->walk_speed = atof(val);
        } else {
            printf("unrecognized parameter: key=%s val=%s\n", key, val);
        }
    }
    return true;
}

void router_request_dump(router_request_t *req) {
    printf("from: %d\n"
           "to: %d\n"
           "time: %d\n"
           "speed: %f\n", req->from, req->to, req->time, req->walk_speed);
}

