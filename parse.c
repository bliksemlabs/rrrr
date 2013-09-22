#include "parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void parse_request(router_request_t *req, tdata_t *tdata, int opt, char *optarg) {
    const char delim[2] = ",";
    char *token;

    switch (opt) {
    case 'a':
        req->arrive_by = true;
        break;
    case 'd':
        req->arrive_by = false;
        break;
    case 'r':
        srand(time(NULL));
        router_request_randomize(req);
        break;
    case 'D':
        {
            struct tm ltm;
            memset (&ltm, 0, sizeof(struct tm));
            strptime (optarg, "%Y-%m-%dT%H:%M:%S", &ltm);
            router_request_from_epoch (req, tdata, mktime(&ltm)); // from struct_tm instead?
        }
        break;
    case 'f':
        req->from = strtol(optarg, NULL, 10);
        break;
    case 't':
        req->to = strtol(optarg, NULL, 10);
        break;
    case 'V':
        req->via = strtol(optarg, NULL, 10);
        break;
    case 's':
        req->walk_slack = strtol(optarg, NULL, 10);
        break;
    case 'S':
        req->walk_speed = strtod(optarg, NULL);
        break;
    case 'o':
        req->optimise = 0;
        token = strtok(optarg, delim);

        while ( token != NULL ) {
            if (strcmp(token, "shortest")  == 0) req->optimise |= o_shortest;
            if (strcmp(token, "transfers") == 0) req->optimise |= o_transfers;
            if (strcmp(token, "all")       == 0) req->optimise  = o_all;
            token = strtok(NULL, delim);
        }
        break;
    case 'm':
        req->mode = 0;
        token = strtok(optarg, delim);

        while ( token  != NULL ) {
            if (strcmp(token, "tram") == 0)      req->mode |= m_tram;
            if (strcmp(token, "subway") == 0)    req->mode |= m_subway;
            if (strcmp(token, "rail") == 0)      req->mode |= m_rail;
            if (strcmp(token, "bus") == 0)       req->mode |= m_bus;
            if (strcmp(token, "ferry") == 0)     req->mode |= m_ferry;
            if (strcmp(token, "cablecar") == 0)  req->mode |= m_cablecar;
            if (strcmp(token, "gondola") == 0)   req->mode |= m_gondola;
            if (strcmp(token, "funicular") == 0) req->mode |= m_funicular;
            if (strcmp(token, "all") == 0)       req->mode = m_all;

            token = strtok(NULL, delim);
        }
        break;
    case 'A':
        token = strtok(optarg, delim);

        while ( token != NULL ) {
            if (strcmp(token, "accessible") == 0) req->trip_attributes |= ta_accessible;
            if (strcmp(token, "toilet")     == 0) req->trip_attributes |= ta_toilet;
            if (strcmp(token, "wifi")       == 0) req->trip_attributes |= ta_wifi;
            if (strcmp(token, "none")       == 0) req->trip_attributes =  ta_none;
            token = strtok(NULL, delim);
        }
        break;
    case 'x':
        token = strtok(optarg, delim);
        while ( token  != NULL ) {
            if (strlen(token) > 0) {
                long int tmp = strtol(token, NULL, 10);
                if (tmp >= 0) {
                    req->banned_route = tmp;
                    req->n_banned_routes = 1;
                }
            }

            token = strtok(NULL, delim);
        }
        break;
    case 'y':
        token = strtok(optarg, delim);
        while ( token  != NULL ) {
            if (strlen(token) > 0) {
                long int tmp = strtol(token, NULL, 10);
                if (tmp >= 0) {
                    req->banned_stop = tmp;
                    req->n_banned_stops = 1;
                }
            }

            token = strtok(NULL, delim);
        }
        break;
    case 'z':
        token = strtok(optarg, delim);
        while ( token  != NULL ) {
            if (strlen(token) > 0) {
                long int tmp_route = strtol(token, NULL, 10);
                if (tmp_route >= 0) {
                    token = strtok(NULL, delim);
                    long int tmp_trip = strtol(token, NULL, 10);

                    if (tmp_trip >= 0) {
                        req->banned_trip_route = tmp_route;
                        req->banned_trip_offset = tmp_trip;
                        req->n_banned_trips = 1;
                    }
                }
            }

            token = strtok(NULL, delim);
        }
        break;
    case 'Q': {
        uint32_t tmp_route = NONE;
        uint32_t tmp_trip  = NONE;
        for (token = strtok(optarg, delim); token != NULL; token = strtok(NULL, delim)) {
            if (strlen(token) > 0) {
                if (tmp_route != NONE) {
                    tmp_trip  = strtol(token, NULL, 10);
                } else {
                    tmp_route = strtol(token, NULL, 10);
                }
            }
        }
        if (tmp_trip != NONE) {
            req->start_trip_route = tmp_route;
            req->start_trip_trip  = tmp_trip;
        }
        break;
    }
    case 'w':
        token = strtok(optarg, delim);
        while ( token  != NULL ) {
            if (strlen(token) > 0) {
                long int tmp = strtol(token, NULL, 10);
                if (tmp >= 0) {
                    req->banned_stop_hard = tmp;
                    req->n_banned_stops_hard = 1;
                }
            }

            token = strtok(NULL, delim);
        }
        break;
    }
}
