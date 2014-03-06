/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

#include "parse.h"
#include "qstring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void parse_request(router_request_t *req, tdata_t *tdata, HashGrid *hg, int opt, char *optarg) {
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
        router_request_randomize(req, tdata);
        break;
    case 'D':
        {
            struct tm ltm;
            memset (&ltm, 0, sizeof(struct tm));
            strptime (optarg, "%Y-%m-%dT%H:%M:%S", &ltm);
            ltm.tm_isdst = -1;
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
    case 'l':
    case 'L':
        if (hg) {
            token = strtok(optarg, delim);
            if (token != NULL) {
                double lat = strtod(token, NULL);
                token = strtok(NULL, delim);
                if (token != NULL) {
                    double lon = strtod(token, NULL);
                    HashGridResult result;
                    coord_t qc;
                    double radius_meters = 150;
                    coord_from_lat_lon (&qc, lat, lon);
                    HashGrid_query (hg, &result, qc, radius_meters);
                    uint32_t item = HashGridResult_closest (&result);
                    if (opt == 'l') {
                        req->from = item;
                    } else {
                        req->to = item;
                    }
                }
            }
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

#define BUFLEN 255
bool parse_request_from_qstring(router_request_t *req, tdata_t *tdata, HashGrid *hg, char *qstring) {
    if (qstring == NULL)
        qstring = "";
    char key[BUFLEN];
    char *val;
    while (qstring_next_pair(qstring, key, &val, BUFLEN)) {
        int opt = 0;
        if (strcmp(key, "from-latlng") == 0) {
            opt = 'l';
        } else if (strcmp(key, "to-latlng") == 0) {
            opt = 'L';
        } else if (strcmp(key, "arrive") == 0) {
            opt = 'a';
        } else if (strcmp(key, "depart") == 0) {
            opt = 'd';
        } else if (strcmp(key, "date") == 0) {
            opt = 'D';
        } else if (strcmp(key, "walk-slack") == 0) {
            opt = 's';
        } else if (strcmp(key, "walk-speed") == 0) {
            opt = 'S';
        } else if (strcmp(key, "optimise") == 0) {
            opt = 'o';
        } else if (strcmp(key, "from-idx") == 0) {
            opt = 'f';
        } else if (strcmp(key, "to-idx") == 0) {
            opt = 't';
        } else if (strcmp(key, "via-idx") == 0) {
            opt = 'V';
        } else if (strcmp(key, "mode") == 0) {
            opt = 'm';
        } else if (strcmp(key, "start-trip-idx") == 0) {
            opt = 'Q';
        } else if (strcmp(key, "bannend-routes-idx") == 0) {
            opt = 'x';
        } else if (strcmp(key, "bannend-stops-idx") == 0) {
            opt = 'y';
        } else if (strcmp(key, "bannend-stops-hard-idx") == 0) {
            opt = 'w';
        } else if (strcmp(key, "bannend-trips-idx") == 0) {
            opt = 'z';
        } else if (strcmp(key, "trip-attributes") == 0) {
            opt = 'A';
        } else if (strcasecmp(key, "showIntermediateStops") == 0 && strcasecmp(val, "true") == 0) {
            req->intermediatestops = true;
            continue;
        } else {
            printf("unrecognized parameter: key=%s val=%s\n", key, val);
            continue;
        }
        parse_request(req, tdata, hg, opt, val);
    }

    if (req->time == UNREACHED) {
        struct tm ltm;
        memset (&ltm, 0, sizeof(struct tm));
        time_t now = time(0);
        ltm = *localtime(&now);
        router_request_from_epoch (req, tdata, mktime(&ltm));
    }

    if (req->time_rounded && !(req->arrive_by)) {
        req->time++;    // Round time upwards when departing after a requested time
    }
    req->time_rounded = false;

    return true;
}

