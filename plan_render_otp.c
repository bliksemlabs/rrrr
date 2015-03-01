/* Copyright 2013–2015 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* plan_render_otp.c renders plan structs to a json-variant of the API output of the OpenTripPlanner project */
#include "json.h"
#include "util.h"
#include "polyline.h"
#include "plan_render_otp.h"
#include "router_request.h"
#include <stdlib.h>

static char *
modes_string (tmode_t m, char *dst) {
    if ((m & m_tram)      == m_tram)      { dst = strncpy(dst, "TRAM,", 5);       dst += 5; }
    if ((m & m_subway)    == m_subway)    { dst = strncpy(dst, "SUBWAY,", 7);     dst += 7; }
    if ((m & m_rail)      == m_rail)      { dst = strncpy(dst, "RAIL,", 5);       dst += 5; }
    if ((m & m_bus)       == m_bus)       { dst = strncpy(dst, "BUS,", 4);        dst += 4; }
    if ((m & m_ferry)     == m_ferry)     { dst = strncpy(dst, "FERRY,", 6);      dst += 6; }
    if ((m & m_cablecar)  == m_cablecar)  { dst = strncpy(dst, "CABLE_CAR,", 10); dst += 10; }
    if ((m & m_gondola)   == m_gondola)   { dst = strncpy(dst, "GONDOLA,", 8);    dst += 8; }
    if ((m & m_funicular) == m_funicular) { dst = strncpy(dst, "FUNICULAR,", 10); dst += 10; }
    return dst;
}

/* Produces a polyline connecting a subset of the stops in a route,
 * or connecting two walk path endpoints if route_idx == WALK.
 * sidx0 and sidx1 are global stop indexes, not stop indexes within the route.
 */
static void
polyline_for_leg (polyline_t *pl, tdata_t *tdata, leg_t *leg) {
    polyline_begin(pl);

    if (leg->journey_pattern == WALK) {
        polyline_latlon (pl, tdata->stop_point_coords[leg->sp_from]);
        polyline_latlon (pl, tdata->stop_point_coords[leg->sp_to]);
    } else {
        jppidx_t i_jpp;
        journey_pattern_t jp = tdata->journey_patterns[leg->journey_pattern];
        spidx_t *stops = tdata_points_for_journey_pattern (tdata, leg->journey_pattern);
        bool output = false;
        for (i_jpp = 0; i_jpp < jp.n_stops; ++i_jpp) {
            spidx_t sidx = stops[i_jpp];
            if (!output && (sidx == leg->sp_from)) output = true;
            if (output) polyline_latlon (pl, tdata->stop_point_coords[sidx]);
            if (sidx == leg->sp_to) break;
        }
    }
    /* printf ("final polyline: %s\n\n", polyline_result ()); */
}

static int64_t
rtime_to_msec(rtime_t rtime, time_t date, int32_t tdata_utc_offset) {
    return ((int64_t) 1000) * (RTIME_TO_SEC_SIGNED(rtime - RTIME_ONE_DAY) + date - tdata_utc_offset);
}

static void
json_place (json_t *j, const char *key, rtime_t arrival, rtime_t departure,
            spidx_t stop_index, tdata_t *tdata, time_t date) {
    const char *stop_name = tdata_stop_point_name_for_index(tdata, stop_index);
    const char *platformcode = tdata_platformcode_for_index(tdata, stop_index);
    const char *stop_id = tdata_stop_point_id_for_index(tdata, stop_index);
    uint8_t *stop_attr = tdata_stop_point_attributes_for_index(tdata, stop_index);
    latlon_t coords = tdata->stop_point_coords[stop_index];
    json_key_obj(j, key);
        json_kv(j, "name", stop_name);
        json_key_obj(j, "stopId");
            json_kv(j, "agencyId", "NL");
            json_kv(j, "id", stop_id);
        json_end_obj(j);
        json_kv(j, "stopCode", NULL); /* eventually fill it with UserStopCode */
        json_kv(j, "platformCode", platformcode);
        json_kf(j, "lat", coords.lat);
        json_kf(j, "lon", coords.lon);
        json_kv(j, "wheelchairBoarding", (*stop_attr & sa_wheelchair_boarding) ? "true" : NULL);
        json_kv(j, "visualAccessible", (*stop_attr & sa_visual_accessible) ? "true" : NULL);
	if (arrival == UNREACHED) {
        	json_kv(j, "arrival", NULL);
    } else {
        	json_kl(j, "arrival", rtime_to_msec(arrival, date, tdata->utc_offset));
    }

	if (departure == UNREACHED) {
		json_kv(j, "departure", NULL);
	} else {
		json_kl(j, "departure", rtime_to_msec(departure, date, tdata->utc_offset));
    }
    json_end_obj(j);
}

static void put_servicedate(leg_t *leg, time_t date, char *servicedate){
    struct tm ltm;
    time_t servicedate_time = date + (SEC_IN_ONE_DAY * (leg->t0 % RTIME_ONE_DAY));
    rrrr_gmtime_r(&servicedate_time, &ltm);
    strftime(servicedate, 9, "%Y%m%d", &ltm);
}

static void
json_leg (json_t *j, leg_t *leg, tdata_t *tdata,
          router_request_t *req, time_t date) {
    const char *mode = NULL;
    const char *headsign = NULL;
    const char *linecode = NULL;
    const char *line_color = NULL;
    const char *line_color_text = NULL;
    const char *linename = NULL;
    const char *commercialmode = NULL;
    const char *line_id = NULL;
    const char *vj_id = NULL;
    vj_attribute_mask_t vj_attributes = 0;
    const char *wheelchair_accessible = NULL;
    const char *operator_id = NULL;
    const char *operator_name = NULL;
    const char *operator_url = NULL;
    char agencyTzOffset[16] = "\0";

    char servicedate[9] = "\0";
    int64_t departuredelay = 0;

    int64_t starttime = rtime_to_msec(leg->t0, date, tdata->utc_offset);
    int64_t endtime = rtime_to_msec(leg->t1, date, tdata->utc_offset);

    polyline_t pl;

    if (leg->journey_pattern == WALK) mode = "WALK"; else {
        put_servicedate(leg, date, servicedate);

        #ifdef RRRR_FEATURE_REALTIME_EXPANDED
        headsign = tdata_headsign_for_journey_pattern_point(tdata, leg->journey_pattern,leg->jpp0);
        #else
        headsign = tdata_headsign_for_journey_pattern(tdata, leg->journey_pattern);
        #endif
        linecode = tdata_line_code_for_journey_pattern(tdata, leg->journey_pattern);
        line_color = tdata_line_color_for_journey_pattern(tdata, leg->journey_pattern);
        line_color_text = tdata_line_color_text_for_journey_pattern(tdata, leg->journey_pattern);
        linename = tdata_line_name_for_journey_pattern(tdata, leg->journey_pattern);
        commercialmode = tdata_commercial_mode_name_for_journey_pattern(tdata, leg->journey_pattern);
        line_id = tdata_line_id_for_journey_pattern(tdata, leg->journey_pattern);
        operator_id = tdata_operator_id_for_journey_pattern(tdata, leg->journey_pattern);
        operator_name = tdata_operator_name_for_journey_pattern(tdata, leg->journey_pattern);
        operator_url = tdata_operator_url_for_journey_pattern(tdata, leg->journey_pattern);
        vj_id = tdata_vehicle_journey_id_for_jp_vj_index(tdata, leg->journey_pattern, leg->vj);
        vj_attributes = tdata->vjs[leg->vj].vj_attributes;
        sprintf(agencyTzOffset,"%d",tdata_utc_offset_for_jp_vj_index(tdata, leg->journey_pattern, leg->vj)*1000);

        /* departuredelay = tdata_delay_min (tdata, leg->journey_pattern, leg->vj); */

        wheelchair_accessible = (vj_attributes & vja_wheelchair_accessible) ? "true" : NULL;
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_tram)      == m_tram)      mode = "TRAM";      else
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_subway)    == m_subway)    mode = "SUBWAY";    else
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_rail)      == m_rail)      mode = "RAIL";      else
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_bus)       == m_bus)       mode = "BUS";       else
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_ferry)     == m_ferry)     mode = "FERRY";     else
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_cablecar)  == m_cablecar)  mode = "CABLE_CAR"; else
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_gondola)   == m_gondola)   mode = "GONDOLA";   else
        if ((tdata->journey_patterns[leg->journey_pattern].attributes & m_funicular) == m_funicular) mode = "FUNICULAR"; else
        mode = "INVALID";
    }

    json_obj(j); /* one leg */
        /* TODO We should have stop arrival/departure here */
        json_place(j, "from", UNREACHED, leg->t0, leg->sp_from, tdata, date);
        json_place(j, "to",   leg->t1, UNREACHED, leg->sp_to, tdata, date);

        json_kv(j, "mode", mode);
        json_kl(j, "startTime", starttime);
        json_kl(j, "endTime",   endtime);
        json_kl(j, "departureDelay", departuredelay);
        json_kl(j, "arrivalDelay", 0);
        json_kv(j, "route", linecode && strcmp(linecode,"") ? linename : linecode);
        json_kv(j, "routeShortName", linecode);
        json_kv(j, "routeLongName", linename);
        json_kv(j, "routeId", line_id);
        json_kv(j, "routeColor", line_color);
        json_kv(j, "routeTextColor", line_color_text);
        json_kv(j, "headsign", headsign);
        json_kv(j, "tripId", vj_id);
        json_kv(j, "serviceDate", servicedate);
        json_kv(j, "agencyId", operator_id);
        json_kv(j, "agencyName", operator_name);
        json_kv(j, "agencyUrl", operator_url);
        if (leg->journey_pattern != WALK){
            json_kv(j, "agencyTimeZoneOffset", agencyTzOffset);
        }
        json_kv(j, "wheelchairAccessible", wheelchair_accessible);
        json_kv(j, "productCategory", commercialmode);
/*
    "realTime": false,
    "distance": 2656.2383456335,
    "mode": "BUS",
    "route": "39",
    "agencyName": "RET",
    "agencyUrl": "http:\/\/www.ret.nl",
    "agencyTimeZoneOffset": 7200000,
    "routeColor": null,
    "routeType": 3,
    "routeId": "1836",
    "routeTextColor": null,
    "interlineWithPreviousLeg": false,
    "tripShortName": "48562",
    "tripBlockId": null,
    "headsign": "Rotterdam Centraal",
    "agencyId": "RET",
    "tripId": "2597372",
    "serviceDate": "20130819",
    "from": {
      "name": "Rotterdam, Nieuwe Crooswijksewe",
      "stopId": {
        "agencyId": "ARR",
        "id": "52272"
      },
      "stopCode": "HA2286",
      "platformCode": null,
      "lon": 4.49654,
      "lat": 51.934423,
      "arrival": 1376897759000,
      "departure": 1376897760000,
      "orig": null,
      "zoneId": null,
      "stopIndex": 3
    },
    "to": {
      "name": "Rotterdam, Rotterdam Centraal",
      "stopId": {
        "agencyId": "ARR",
        "id": "51175"
      },
      "stopCode": "HA3940",
      "platformCode": null,
      "lon": 4.467403,
      "lat": 51.923529,
      "arrival": 1376898480000,
      "departure": 1376898770000,
      "orig": null,
      "zoneId": null,
      "stopIndex": 10
    },
    "legGeometry": {
      "points": "cm~{HkfmZfI|JDfj@zMfHpUlHpI`\\nDzZdChr@",
      "levels": null,
      "length": 8
    },
    "notes": null,
    "alerts": null,
    "routeShortName": "39",
    "routeLongName": null,
    "boardRule": null,
    "alightRule": null,
    "rentedBike": false,
    "transitLeg": true,
    "duration": 720000,
    "intermediateStops": null,
    "steps": [

    ]
*/
        json_key_obj(j, "legGeometry");
            polyline_for_leg (&pl, tdata, leg);
            json_kv(j, "points", polyline_result(&pl));
            json_kv(j, "levels", NULL);
            json_kd(j, "length", (int) polyline_length(&pl));
        json_end_obj(j);
        json_key_arr(j, "intermediateStops");
        if (req->intermediatestops && leg->journey_pattern != WALK) {
            jppidx_t i_jpp;
            bool visible = false;
            for (i_jpp = 0; i_jpp < tdata->journey_patterns[leg->journey_pattern].n_stops; i_jpp++) {
                spidx_t stop_idx = tdata->journey_pattern_points[tdata->journey_patterns[leg->journey_pattern].journey_pattern_point_offset + i_jpp];
                if (stop_idx == leg->sp_from) {
                    visible = true;
                    continue;
                } else if (stop_idx == leg->sp_to) {
                    visible = false;
                    break;
                }

                if (visible) {
                    vehicle_journey_t vj = tdata->vjs[tdata->journey_patterns[leg->journey_pattern].vj_offset + leg->vj];
                    rtime_t arrival = vj.begin_time + tdata->stop_times[vj.stop_times_offset + i_jpp].arrival;
                    rtime_t departure = vj.begin_time + tdata->stop_times[vj.stop_times_offset + i_jpp].departure;

                    json_place(j, NULL, arrival, departure, stop_idx, tdata, date);
                }
            }
        }
        json_end_arr(j);
        json_kl(j, "duration", endtime - starttime);
    json_end_obj(j);
}

static void
json_itinerary (json_t *j, itinerary_t *itin, tdata_t *tdata, router_request_t *req, time_t date) {
    int64_t starttime = rtime_to_msec(itin->legs[0].t0, date, tdata->utc_offset);
    int64_t endtime = rtime_to_msec(itin->legs[(itin->n_legs - 1)].t1, date, tdata->utc_offset);
    int32_t walktime = 0;
    int32_t walkdistance = 0;
    int32_t waitingtime = 0;
    int32_t transittime = 0;
    leg_t *leg;

    json_obj(j); /* one itinerary */
        json_kl(j, "duration", endtime - starttime);
        json_kl(j, "startTime", starttime);
        json_kl(j, "endTime", endtime);
        json_kd(j, "transfers", itin->n_legs / 2 - 1);
        json_key_arr(j, "legs");
            for (leg = itin->legs; leg < itin->legs + itin->n_legs; ++leg) {
                uint32_t leg_duration = RTIME_TO_SEC(leg->t1 - leg->t0);
                json_leg (j, leg, tdata, req, date);
                if (leg->journey_pattern == WALK) {
                    if (leg->sp_from == leg->sp_to) {
                        waitingtime += leg_duration;
                    } else {
                        walktime += leg_duration;

                        /* TODO: make this a real distance */
                        walkdistance += leg_duration;
                    }
                } else {
                    transittime += leg_duration;
                }
            }
        json_end_arr(j);
        json_kd(j, "walkTime", walktime);
        json_kd(j, "transitTime", transittime);
        json_kd(j, "waitingTime", waitingtime);
        json_kd(j, "walkDistance", walkdistance);
        json_kb(j, "walkLimitExceeded", false);
        json_kd(j, "elevationLost",0);
        json_kd(j, "elevationGained",0);
        json_kv(j, "occupancyStatus", NULL);

    json_end_obj(j);
}
static uint32_t
otp_json(json_t *j, plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen) {
    struct tm ltm;
    time_t date_seconds = router_request_to_date(& plan->req, tdata, &ltm);
    char date[11];
    char requesttime[16];
    uint8_t i;
    strftime(date, 11, "%Y-%m-%d", &ltm);
    btimetext(plan->req.time, requesttime);

    json_init(j, buf, buflen);
    json_obj(j);
        json_kv(j, "error", "null");
        json_key_obj(j, "requestParameters");
            json_kv(j, "time", requesttime);
            json_kb(j, "arriveBy", plan->req.arrive_by);
            json_kf(j, "maxWalkDistance", 2000.0);
            json_kv(j, "fromPlace", tdata_stop_point_name_for_index(tdata, plan->req.from_stop_point));
            json_kv(j, "toPlace",   tdata_stop_point_name_for_index(tdata, plan->req.to_stop_point));
            json_kv(j, "date", date);
            if (plan->req.mode == m_all) {
                json_kv(j, "mode", "TRANSIT,WALK");
            } else {
                /* max length is 58 + 4 + 8 = 70, minus shortest (3 + 1) + 1 */
                char modes[67];
                char *dst = modes;

                dst = modes_string (plan->req.mode, dst);
                dst = strcpy(dst, "WALK");

                json_kv(j, "mode", modes);
            }
        json_end_obj(j);
        json_key_obj(j, "plan");
            json_kl(j, "date", ((int64_t) 1000) * date_seconds);
            json_place(j, "from", UNREACHED, UNREACHED, plan->req.from_stop_point, tdata, date_seconds);
            json_place(j, "to", UNREACHED, UNREACHED, plan->req.to_stop_point, tdata, date_seconds);
            json_key_arr(j, "itineraries");
                for (i = 0; i < plan->n_itineraries; ++i) {
                    json_itinerary (j, plan->itineraries + i, tdata, &plan->req, date_seconds);
                }
            json_end_arr(j);
        json_end_obj(j);
        #if 0
        json_key_obj(j, "debug");
            json_kd(j, "precalculationTime", 12);
            json_kd(j, "pathCalculationTime", 808);
            json_kb(j, "timedOut", false);
        json_end_obj(j);
        #endif
    json_end_obj(j);
    /* json_dump(j); */
    return (uint32_t) json_length(j);
}

uint32_t
plan_render_otp(plan_t *plan, tdata_t *tdata, char *buf, uint32_t buflen) {
    json_t j;

    return otp_json(&j, plan, tdata, buf, buflen);
}

uint32_t metadata_render_otp (tdata_t *tdata, char *buf, uint32_t buflen) {
    json_t j;
    latlon_t ll, ur, c;
    float *lon, *lat;
    uint64_t starttime, endtime;
    spidx_t i_stop = (spidx_t) tdata->n_stop_points;
    tmode_t m;

    char modes[67];
    char *dst = modes;

    lon = (float *) malloc(sizeof(float) * tdata->n_stop_points);
    lat = (float *) malloc(sizeof(float) * tdata->n_stop_points);

    if (!lon || !lat) return 0;

    do {
        i_stop--;
        lon[i_stop] = tdata->stop_point_coords[i_stop].lon;
        lat[i_stop] = tdata->stop_point_coords[i_stop].lat;
    } while (i_stop > 0);

    c.lon = median (lon, tdata->n_stop_points, &ll.lon, &ur.lon);
    c.lat = median (lat, tdata->n_stop_points, &ll.lat, &ur.lat);

    free (lon);
    free (lat);

    tdata_validity (tdata, &starttime, &endtime);
    tdata_modes (tdata, &m);

    json_init(&j, buf, buflen);
    json_obj(&j);
    json_kl(&j, "startTime", (int64_t) (starttime * 1000));
    json_kl(&j, "endTime",   (int64_t) (endtime * 1000));

    json_kf(&j, "lowerLeftLatitude", ll.lat);
    json_kf(&j, "lowerLeftLongitude", ll.lon);
    json_kf(&j, "upperRightLatitude", ur.lat);
    json_kf(&j, "upperRightLongitude", ur.lon);
    json_kf(&j, "minLatitude", ll.lat);
    json_kf(&j, "minLongitude", ll.lon);
    json_kf(&j, "maxLatitude", ur.lat);
    json_kf(&j, "maxLongitude", ur.lon);

    json_kf(&j, "centerLatitude", c.lat);
    json_kf(&j, "centerLongitude", c.lon);

    dst = modes_string (m, dst);
    dst[-1] = '\0';

    json_kv(&j, "transitModes", modes);
    json_end_obj(&j);

    return (uint32_t) json_length(&j);
}
