#ifndef _RRRR_TYPES_H
#define _RRRR_TYPES_H

#include "config.h"
#include "hashgrid.h"

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 2^16 / 60 / 60 is 18.2 hours at one-second resolution.
 * By right-shifting times one bit, we get 36.4 hours (over 1.5 days)
 * at 2 second resolution. By right-shifting times two bits, we get
 * 72.8 hours (over 3 days) at 4 second resolution. Three days is just enough
 * to model yesterday, today, and tomorrow for overnight searches, and can
 * also represent the longest rail journeys in Europe.
 */
typedef uint16_t rtime_t;

typedef uint16_t spidx_t;

typedef uint16_t jpidx_t;

typedef uint16_t jp_vjoffset_t;

typedef uint16_t jppidx_t;

typedef uint32_t calendar_t;

typedef struct service_day {
    calendar_t mask;
    rtime_t  midnight;
    bool     apply_realtime;
} serviceday_t;

typedef struct list list_t;
struct list {
    void *list;
    uint32_t size;
    uint32_t len;
};

typedef enum vehicle_journey_attributes {
    vja_none = 0,
    vja_accessible = 1,
    vja_toilet = 2,
    vja_wifi = 4
} vehicle_journey_attributes_t;


typedef enum optimise {
    /* output only shortest time */
    o_shortest  =   1,

    /* output least transfers */
    o_transfers =   2,

    /* output all rides */
    o_all       = 255
} optimise_t;


typedef enum tmode {
    m_tram      =   1,
    m_subway    =   2,
    m_rail      =   4,
    m_bus       =   8,
    m_ferry     =  16,
    m_cablecar  =  32,
    m_gondola   =  64,
    m_funicular = 128,
    m_all       = 255
} tmode_t;

typedef struct router_request router_request_t;
struct router_request {
#ifdef RRRR_FEATURE_LATLON
    /* actual origin in wgs84 presented to the planner */
    latlon_t from_latlon;
    hashgrid_result_t from_hg_result;

    /* actual destination in wgs84 presented to the planner */
    latlon_t to_latlon;
    hashgrid_result_t to_hg_result;

    /* actual intermediate in wgs84 presented to the planner */
    latlon_t via_latlon;
    hashgrid_result_t via_hg_result;
#endif
    /* (nearest) start stop_point index from the users perspective */
    spidx_t from_stop_point;

    /* (nearest) destination stop_point index from the users perspective */
    spidx_t to_stop_point;

    /* preferred transfer stop_point index from the users perspective */
    spidx_t via_stop_point;

    /* onboard departure, journey_pattern index from the users perspective */
    jpidx_t onboard_journey_pattern;

    /* onboard departure, vehicle_journey offset within the journey_pattern */
    uint32_t onboard_journey_pattern_vjoffset;

    /* TODO comment on banning */
    #if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
    jpidx_t banned_journey_patterns[RRRR_MAX_BANNED_JOURNEY_PATTERNS];
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS > 0
    spidx_t banned_stops[RRRR_MAX_BANNED_STOP_POINTS];
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
    spidx_t banned_stop_points_hard[RRRR_MAX_BANNED_STOP_POINTS_HARD];
    #endif
    #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
    jpidx_t banned_vjs_journey_pattern[RRRR_MAX_BANNED_VEHICLE_JOURNEYS];
    jp_vjoffset_t banned_vjs_offset[RRRR_MAX_BANNED_VEHICLE_JOURNEYS];
    #endif

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

#ifdef FEATURE_OPERATOR_FILTER
    /* Filter the journey_patterns by the operating operator */
    uint16_t operator;
#endif

    /* the largest number of transfers to allow in the result */
    uint8_t max_transfers;

    /* select the requested modes by a bitfield */
    uint8_t mode;

    /* an extra delay per transfer, in seconds */
    uint8_t walk_slack;

    /* select the required vehicle_journey attributes by a bitfield */
    uint8_t vj_attributes;

    /* TODO comment on banning */
    #if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
    uint8_t n_banned_journey_patterns;
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS > 0
    uint8_t n_banned_stops;
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
    uint8_t n_banned_stop_points_hard;
    #endif
    #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
    uint8_t n_banned_vjs;
    #endif

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


#ifndef _LP64
    #define ZU "%u"
#else
    #define ZU "%lu"
#endif

#define SEC_TO_RTIME(x) (rtime_t) ((x) >> 2)
#define RTIME_TO_SEC(x) (((uint32_t)x) << 2)
#define RTIME_TO_SEC_SIGNED(x) ((x) << 2)

#define SEC_IN_ONE_MINUTE (60)
#define SEC_IN_ONE_HOUR   (60 * SEC_IN_ONE_MINUTE)
#define SEC_IN_ONE_DAY    (24 * SEC_IN_ONE_HOUR)
#define SEC_IN_TWO_DAYS   (2 * SEC_IN_ONE_DAY)
#define SEC_IN_THREE_DAYS (3 * SEC_IN_ONE_DAY)
#define RTIME_ONE_DAY     (SEC_TO_RTIME(SEC_IN_ONE_DAY))
#define RTIME_TWO_DAYS    (SEC_TO_RTIME(SEC_IN_TWO_DAYS))
#define RTIME_THREE_DAYS  (SEC_TO_RTIME(SEC_IN_THREE_DAYS))

#define UNREACHED UINT16_MAX
#define NONE      (UINT16_MAX)
#define WALK      (UINT16_MAX - 1)

#define STOP_NONE ((spidx_t) -1)
#define ONBOARD   ((spidx_t) -2)

#endif /* _RRRR_TYPES */
