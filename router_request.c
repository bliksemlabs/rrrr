/* Copyright 2013–2015 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "config.h"
#include "router_request.h"
#include "util.h"

#include <stdio.h>
#include <assert.h>

/* router_request_to_epoch returns the time-date
 * used in the request in seconds since epoch.
 */
time_t
router_request_to_epoch (router_request_t *req, tdata_t *tdata,
                         struct tm *tm_out) {
    time_t seconds;
    calendar_t day_mask = req->day_mask;
    uint8_t cal_day = 0;

    while (day_mask >>= 1) cal_day++;

    seconds = (time_t) (tdata->calendar_start_time + (cal_day * SEC_IN_ONE_DAY) ) +
              RTIME_TO_SEC(req->time - RTIME_ONE_DAY) - tdata->utc_offset;

    rrrr_gmtime_r (&seconds, tm_out);
    return seconds;
}


/* router_request_to_date returns the UTC date used
 * in the request in seconds since epoch.
 */
time_t
router_request_to_date (router_request_t *req, tdata_t *tdata,
                        struct tm *tm_out) {
    time_t seconds;
    calendar_t day_mask = req->day_mask;
    uint8_t cal_day = 0;

    while (day_mask >>= 1) cal_day++;

    seconds = (time_t) (tdata->calendar_start_time + (cal_day * SEC_IN_ONE_DAY));

    rrrr_gmtime_r (&seconds, tm_out);
    return seconds;
}

/* router_request_initialize sets all the defaults for the request.
 * it will not set required arguments such as arrival and departure
 * stops, not it will set the time.
 */
void
router_request_initialize(router_request_t *req) {
    req->exit.n_points = 0;
    req->entry.n_points = 0;
    req->walk_speed = RRRR_DEFAULT_WALK_SPEED;
    req->walk_slack = RRRR_DEFAULT_WALK_SLACK;
    req->walk_max_distance = RRRR_DEFAULT_WALK_MAX_DISTANCE;
    req->from_stop_point = req->to_stop_point = req->via_stop_point = STOP_NONE;
    req->from_stop_area = req->to_stop_area = STOP_NONE;
    req->time = UNREACHED;
    req->time_cutoff = UNREACHED;
    req->arrive_by = true;
    req->time_rounded = false;
    req->calendar_wrapped = false;
    req->max_transfers = RRRR_DEFAULT_MAX_ROUNDS - 1;
    req->mode = m_all;
    req->vj_attributes = vja_none;
    req->optimise = o_all;
    #if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
    req->n_banned_journey_patterns = 0;
    rrrr_memset (req->banned_journey_patterns, NONE, RRRR_MAX_BANNED_JOURNEY_PATTERNS);
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS > 0
    req->n_banned_stops = 0;
    rrrr_memset (req->banned_stops, STOP_NONE, RRRR_MAX_BANNED_STOP_POINTS);
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
    req->n_banned_stop_points_hard = 0;
    rrrr_memset (req->banned_stop_points_hard, STOP_NONE, RRRR_MAX_BANNED_STOP_POINTS_HARD);
    #endif
    #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
    req->n_banned_vjs = 0;
    rrrr_memset (req->banned_vjs_journey_pattern, NONE, RRRR_MAX_BANNED_VEHICLE_JOURNEYS);
    rrrr_memset (req->banned_vjs_offset, 0, RRRR_MAX_BANNED_VEHICLE_JOURNEYS);
    #endif
    req->onboard_journey_pattern = NONE;
    req->onboard_journey_pattern_vjoffset = NONE;
    req->intermediatestops = false;

    req->from_latlon.lat = 0.0;
    req->from_latlon.lon = 0.0;
    req->to_latlon.lat = 0.0;
    req->to_latlon.lon = 0.0;
}

/* Initializes the router request then fills in its time and datemask fields
 * from the given epoch time.
 */
/* TODO: if we set the date mask in the router itself we wouldn't need the tdata here. */
void
router_request_from_epoch(router_request_t *req, tdata_t *tdata,
                          time_t epochtime) {
    #if 0
    char etime[32];
    strftime(etime, 32, "%Y-%m-%d %H:%M:%S\0", localtime(&epochtime));
    fprintf (stderr, "epoch time: %s [%ld]\n", etime, epochtime);
    fprintf (stderr, "calendar_start_time: %s [%ld]\n", etime, tdata->calendar_start_time);
    #endif
    calendar_t cal_day;
    time_t request_time = (epochtime - tdata->calendar_start_time + tdata->utc_offset);

    req->time =  RTIME_ONE_DAY + SEC_TO_RTIME(request_time%SEC_IN_ONE_DAY);
    req->time_rounded = ((request_time%SEC_IN_ONE_DAY) % 4) > 0;
    cal_day = (calendar_t) (request_time/SEC_IN_ONE_DAY);

    if (cal_day >= tdata->n_days ) {
        /* date not within validity period of the timetable file,
         * wrap to validity range 28 is a multiple of 7, so we always wrap
         * up to the same day of the week.
         */
        cal_day %= 28;
        fprintf (stderr, "calendar day out of range. wrapping to %u, "
                         "which is on the same day of the week.\n", cal_day);
        req->calendar_wrapped = true;
    }
    req->day_mask = ((calendar_t) 1) << cal_day;
}

/* router_request_randomize creates a completely filled in, working request.
 */
void
router_request_randomize (router_request_t *req, tdata_t *tdata) {
    req->walk_speed = RRRR_DEFAULT_WALK_SPEED;
    req->walk_slack = RRRR_DEFAULT_WALK_SLACK;
    req->walk_max_distance = RRRR_DEFAULT_WALK_MAX_DISTANCE;
    req->time = RTIME_ONE_DAY + SEC_TO_RTIME(3600 * 9 + rrrrandom(3600 * 12));
    req->via_stop_point = STOP_NONE;
    req->time_cutoff = UNREACHED;
    /* 0 or 1 */
    req->arrive_by = (bool) (rrrrandom(2));
    req->max_transfers = RRRR_DEFAULT_MAX_ROUNDS - 1;
    req->day_mask = ((calendar_t) 1) << rrrrandom(32);
    req->mode = m_all;
    req->vj_attributes = vja_none;
    req->optimise = o_all;
    #if RRRR_MAX_BANNED_JOURNEY_PATTERNS > 0
    req->n_banned_journey_patterns = 0;
    rrrr_memset (req->banned_journey_patterns, NONE, RRRR_MAX_BANNED_JOURNEY_PATTERNS);
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS > 0
    req->n_banned_stops = 0;
    rrrr_memset (req->banned_stops, STOP_NONE, RRRR_MAX_BANNED_STOP_POINTS);
    #endif
    #if RRRR_MAX_BANNED_STOP_POINTS_HARD > 0
    req->n_banned_stop_points_hard = 0;
    rrrr_memset (req->banned_stop_points_hard, STOP_NONE, RRRR_MAX_BANNED_STOP_POINTS_HARD);
    #endif
    #if RRRR_MAX_BANNED_VEHICLE_JOURNEYS > 0
    req->n_banned_vjs = 0;
    rrrr_memset (req->banned_vjs_journey_pattern, NONE, RRRR_MAX_BANNED_VEHICLE_JOURNEYS);
    rrrr_memset (req->banned_vjs_offset, 0, RRRR_MAX_BANNED_VEHICLE_JOURNEYS);
    #endif
    req->intermediatestops = false;
    req->from_stop_point = (spidx_t) rrrrandom(tdata->n_stop_points);
    req->to_stop_point = (spidx_t) rrrrandom(tdata->n_stop_points);

    req->from_latlon = tdata->stop_point_coords[rrrrandom(tdata->n_stop_points)];
    req->to_latlon = tdata->stop_point_coords[rrrrandom(tdata->n_stop_points)];
    req->from_stop_point = STOP_NONE;
    req->to_stop_point = STOP_NONE;
}

static bool
best_sp_by_round (router_t *router, router_request_t *req, uint8_t round, rtime_t *time) {
    uint64_t offset_state = router->tdata->n_stop_points * round;
    spidx_t sp_index;
    spidx_t best_sp_index = STOP_NONE;
    rtime_t best_time = (rtime_t) (req->arrive_by ? 0 : UNREACHED);
    rtime_t *round_best_time = &router->states_time[offset_state];

    street_network_t target = req->arrive_by ? req->entry : req->exit;
    int32_t i_target = target.n_points;

    while (i_target){
        --i_target;
        sp_index = target.stop_points[i_target];
        if (round_best_time[sp_index] != UNREACHED) {
            if (req->arrive_by) {
                if (round_best_time[sp_index] - target.durations[i_target] > best_time) {
                    best_sp_index = (spidx_t) sp_index;
                    best_time = round_best_time[sp_index] - target.durations[i_target];
                }
            } else {
                if (round_best_time[sp_index] + target.durations[i_target] < best_time) {
                    best_sp_index = (spidx_t) sp_index;
                    best_time = round_best_time[sp_index] + target.durations[i_target];
                }
            }
        }
    }

    *time = best_time;

    if (best_sp_index != STOP_NONE) {
        if (round > 0) {
            offset_state -= router->tdata->n_stop_points;
            if (best_time >= router->states_time[offset_state + best_sp_index]) {
                fprintf(stderr, "A later round shows solutions which are not better.\n");
                return false;
            }
        }
        return true;
    }

    return false;
}

static void
reverse_request (router_request_t *req, uint8_t round, rtime_t best_time) {
    req->time_cutoff = req->time;
    req->time = best_time;

    req->max_transfers = round;
    req->arrive_by = !(req->arrive_by);
}

bool
router_request_reverse_all(router_t *router, router_request_t *req, router_request_t *ret, uint8_t *ret_n) {
    rtime_t best_time;
    int8_t round;

    assert (req->max_transfers <= RRRR_DEFAULT_MAX_ROUNDS);

    round = (int8_t) req->max_transfers;

    do {
        if (best_sp_by_round(router, req, (uint8_t) round, &best_time)) {
            ret[*ret_n] = *req;
            reverse_request(&ret[*ret_n], (uint8_t) round, best_time);
            (*ret_n)++;
        }
        round--;
    } while (round >= 0);

    return (*ret_n > 0);
}

/* Reverse the direction of the search leaving most request parameters
 * unchanged but applying time and transfer cutoffs based on an existing
 * result for the same request. Returns a boolean value indicating whether
 * the request was successfully reversed.
 */
bool
router_request_reverse(router_t *router, router_request_t *req) {
    spidx_t best_sp_index = NONE;
    uint8_t max_transfers = req->max_transfers;
    uint8_t round = UINT8_MAX;
    rtime_t best_time;
    /* range-check to keep search within states array */
    if (max_transfers >= RRRR_DEFAULT_MAX_ROUNDS)
        max_transfers = RRRR_DEFAULT_MAX_ROUNDS - 1;

    while (max_transfers){
        max_transfers--;
        if (best_sp_by_round(router, req, max_transfers, &best_time)){
            round = (uint8_t) (max_transfers+1);
            break;
        }
    }

    /* In the case that no solution was found,
     * the request will remain unchanged.
     */
    if (round == UINT8_MAX) return false;

    req->time_cutoff = req->time;
    req->time = best_time;
    #if 0
    fprintf (stderr, "State present at round %d \n", round);
    router_state_dump (router, round * router->tdata->n_stop_points + sp_index);
    #endif
    req->max_transfers = round;
    req->arrive_by = !(req->arrive_by);
    #ifdef RRRR_DEBUG
    router_request_dump(req, router->tdata);
    range_check(req, router->tdata);
    /* TODO: range-check the resulting request here? */
    #endif
    return true;

    /* Eigenlijk zou in de counter clockwise stap een walkleg niet naar de
     * target moeten gaan, maar naar de de fictieve arrival / departure halte.
     * Zou mooi zijn om een punt te introduceren die dat faciliteert, dan zou
     * je op dat punt een apply_hashgrid kunnen doen, ipv apply_transfers.
     */
}

/* Check the given request against the characteristics of the router that will
 * be used. Indexes larger than array lengths for the given router, signed
 * values less than zero, etc. can and will cause segfaults and present
 * security risks.
 *
 * We could also infer departure stop_point etc. from start vehicle_journey
 * here, "missing start point" and reversal problems.
 */
bool
range_check(router_request_t *req, tdata_t *tdata) {
    return !(req->walk_speed < 0.1 ||
             req->from_stop_point >= tdata->n_stop_points ||
             req->to_stop_point   >= tdata->n_stop_points ||
             req->from_stop_area   >= tdata->n_stop_areas ||
             req->to_stop_area   >= tdata->n_stop_areas
    );
}

/* router_request_dump prints the current request structure to the screen */
void
router_request_dump(router_request_t *req, tdata_t *tdata) {
    const char *from_sa_id = tdata_stop_area_name_for_index(tdata, req->from_stop_area);
    const char *to_sa_id   = tdata_stop_area_name_for_index(tdata, req->to_stop_area);
    const char *from_sp_id = tdata_stop_point_name_for_index(tdata, req->from_stop_point);
    const char *to_sp_id   = tdata_stop_point_name_for_index(tdata, req->to_stop_point);
    char time[32], time_cutoff[32], date[11];
    struct tm ltm;

    router_request_to_date (req, tdata, &ltm);
    strftime(date, 11, "%Y-%m-%d", &ltm);

    btimetext(req->time, time);
    btimetext(req->time_cutoff, time_cutoff);
    printf("-- Router Request --\n"
            "from_stop_area:  %s [%d]\n"
            "from_stop_point:  %s [%d]\n"
            "from_latlon:  %f,%f\n"
            "to_stop_area:    %s [%d]\n"
            "to_stop_point:    %s [%d]\n"
            "to_latlon:  %f,%f\n"
            "date:  %s\n"
            "time:  %s [%d]\n"
            "speed: %f m/sec\n"
            "arrive-by: %s\n"
            "max xfers: %d\n"
            "max time:  %s\n"
            "mode: ",
            from_sa_id, req->from_stop_area,
            from_sp_id, req->from_stop_point,
            req->from_latlon.lat, req->from_latlon.lon,
            to_sa_id, req->to_stop_area,
            to_sp_id, req->to_stop_point,
            req->to_latlon.lat,req->to_latlon.lon,
            date, time,
            req->time, req->walk_speed,
            (req->arrive_by ? "true" : "false"),
            req->max_transfers, time_cutoff);

    if (req->mode == m_all) {
        printf("transit\n");
    } else {
         if ((req->mode & m_tram)      == m_tram)      printf("tram,");
         if ((req->mode & m_subway)    == m_subway)    printf("subway,");
         if ((req->mode & m_rail)      == m_rail)      printf("rail,");
         if ((req->mode & m_bus)       == m_bus)       printf("bus,");
         if ((req->mode & m_ferry)     == m_ferry)     printf("ferry,");
         if ((req->mode & m_cablecar)  == m_cablecar)  printf("cablecar,");
         if ((req->mode & m_gondola)   == m_gondola)   printf("gondola,");
         if ((req->mode & m_funicular) == m_funicular) printf("funicular,");
         printf("\b\n");
    }
}
