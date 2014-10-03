#ifndef _ROUTER_REQUEST_H
#define _ROUTER_REQUEST_H

#include "stubs.h"
#include "config.h"

typedef struct router_request router_request_t;
struct router_request {
    /* actual origin in wgs84 presented to the planner */
    latlon_t from_latlon;

    /* actual destination in wgs84 presented to the planner */
    latlon_t to_latlon;

    /* actual intermediate in wgs84 presented to the planner */
    latlon_t via_latlon;

    /* (nearest) start stop index from the users perspective */
    uint32_t from;

    /* (nearest) destination stop index from the users perspective */
    uint32_t to;

    /* preferred transfer stop index from the users perspective */
    uint32_t via;

    /* onboard departure, route index from the users perspective */
    uint32_t onboard_trip_route;

    /* onboard departure, trip offset within the route */
    uint32_t onboard_trip_offset;

    /* the largest number of transfers to allow in the result */
    uint32_t max_transfers;

    /* TODO comment on banning */
    uint32_t banned_route[RRRR_MAX_BANNED_ROUTES];
    uint32_t banned_stop[RRRR_MAX_BANNED_STOPS];
    uint32_t banned_stop_hard[RRRR_MAX_BANNED_STOPS];
    uint32_t banned_trip_route[RRRR_MAX_BANNED_TRIPS];
    uint32_t banned_trip_offset[RRRR_MAX_BANNED_TRIPS];

    /* bit for the day on which we are searching, relative to the timetable calendar */
    calendar_t day_mask;

    /* the speed by foot, in meters per second */
    float walk_speed;

    /* the departure or arrival time at which to search, in internal rtime */
    rtime_t time;

    /* the latest (or earliest in arrive_by) acceptable time to reach the destination */
    rtime_t time_cutoff;

    /* the maximum distance the hashgrid will search through for alternative stops */
    uint16_t walk_max_distance;

#ifdef FEATURE_AGENCY_FILTER
    /* Filter the routes by the operating agency */
    uint16_t agency;
#endif

    /* select the requested modes by a bitfield */
    uint8_t mode;

    /* an extra delay per transfer, in seconds */
    uint8_t walk_slack;

    /* select the required trip attributes by a bitfield */
    uint8_t trip_attributes;

    /* TODO comment on banning */
    uint8_t n_banned_routes;
    uint8_t n_banned_stops;
    uint8_t n_banned_stops_hard;
    uint8_t n_banned_trips;

    /* restrict the output to specific optimisation flags */
    uint8_t optimise;

    /* whether the given time is an arrival time rather than a departure time */
    bool arrive_by;

    /* whether the requested date is out of the timetable validity */
    bool calendar_wrapped;

    /* whether the requested time had to be rounded down to fit in an rtime field */
    bool time_rounded;

    /* whether to show the intermediate stops in the output */
    bool intermediatestops;
};

time_t req_to_date (router_request_t *req, tdata_t *tdata, struct tm *tm_out);
time_t req_to_epoch (router_request_t *req, tdata_t *tdata, struct tm *tm_out);

void router_request_initialize(router_request_t *req);
void router_request_from_epoch(router_request_t *req, tdata_t *tdata, time_t epochtime);
void router_request_randomize (router_request_t *req, tdata_t *tdata);
void router_request_next(router_request_t *req, rtime_t inc);
bool router_request_reverse(router_t *router, router_request_t *req);
void router_request_dump(router_t *router, router_request_t *req);

#endif
