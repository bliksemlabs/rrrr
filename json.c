/* json.c */

#include "json.h"
#include "geometry.h"

#include <stdio.h>
#include <string.h>

static bool in_list = false;
static char *buf_start;
static char *buf_end;
static char *b;
static bool overflowed = false;

/* private functions */

/* Check an operation that will write multiple characters to the buffer, when the maximum number of characters is known. */
static bool remaining(size_t n) {
    if (b + n < buf_end) return true;
    overflowed = true;
    return false; 
}

/* Overflow-checked copy of a single char to the buffer. */
static void check(char c) { 
    if (b >= buf_end) overflowed = true;
    else *(b++) = c;
}

/* Add a comma to the buffer, but only if we are currently in a list. */
static void comma() { if (in_list) check(','); }

/* Write a string out to the buffer, surrounding it in quotes and escaping all quotes or slashes. */
static void string (const char *s) {
    if (s == NULL) {
        if (remaining(4)) b += sprintf(b, "null");
        return;
    }
    check('"');
    for (const char *c = s; *c != '\0'; ++c) {
        if (*c == '"' || *c == '/') check('/');
        check(*c);
    }
    check('"');
}

/* Escape a key and copy it to the buffer, preparing for a single value. 
   This should only be used internally, since it sets in_list _before_ the value is added. */
static void ekey (const char *k) {
    comma();
    string(k);
    check(':');
    in_list = true;
}

/* public functions (eventually) */

static void json_begin(char *buf, size_t buflen) { 
    buf_start = b = buf; 
    buf_end = b + buflen - 1;
    in_list = false;
    overflowed = false; 
}

static void json_dump() { 
    *b = '\0'; 
    if (overflowed) printf ("[JSON OVERFLOW]\n");
    printf("%s\n", buf_start); 
}

static size_t json_length() { return b - buf_start; }

static void json_kv(char *key, char *value) {
    ekey(key);
    string(value);
}

static void json_kd(char *key, int value) {
    ekey(key);
    if (remaining(11)) b += sprintf(b, "%d", value);
}

static void json_kf(char *key, double value) {
    ekey(key);
    if (remaining(12)) b += sprintf(b, "%5.5f", value);
}

static void json_kl(char *key, long value) {
    ekey(key);
    if (remaining(21)) b += sprintf(b, "%ld", value);
}

static void json_kb(char *key, bool value) {
    ekey(key);
    if (remaining(5)) b += sprintf(b, value ? "true" : "false");
}

static void json_key_obj(char *key) {
    ekey(key);
    check('{');
    in_list = false;
}

static void json_key_arr(char *key) {
    ekey(key);
    check('[');
    in_list = false;
}

static void json_obj() {
    comma();
    check('{');
    in_list = false;
}

static void json_arr() {
    comma();
    check('[');
    in_list = false;
}

static void json_end_obj() {
    check('}');
    in_list = true;
}

static void json_end_arr() {
    check(']');
    in_list = true;
}

static long rtime_to_msec(rtime_t rtime) { return (rtime << 4) * 1000L; }

static void json_place (char *key, rtime_t arrival, rtime_t departure, uint32_t stop_index, tdata_t *tdata) {
    char *stop_desc = tdata_stop_desc_for_index(tdata, stop_index);
    char *stop_id = tdata_stop_id_for_index(tdata, stop_index);
    latlon_t coords = tdata->stop_coords[stop_index];
    json_key_obj(key);
        json_kv("name", stop_id);
        json_key_obj("stopId");
            json_kv("agencyId", "NL");
            json_kd("id", stop_index);
        json_end_obj();
        json_kd("stopCode", stop_index);
        json_kv("platformCode", NULL);
        json_kf("lat", coords.lat);
        json_kf("lon", coords.lon);
        json_kd("arrival", arrival); // TODO to epoch?
        json_kd("departure", departure); // TODO
    json_end_obj();
}

static void json_leg (struct leg *leg, tdata_t *tdata) {
    char *mode = NULL;
    char *route_id = NULL;

    if (leg->route == WALK) mode = "WALK"; else {
        route_id = tdata_route_desc_for_index(tdata, leg->route);

        if ((tdata->routes[leg->route].attributes & m_tram)      == m_tram)      mode = "TRAM";      else
        if ((tdata->routes[leg->route].attributes & m_subway)    == m_subway)    mode = "SUBWAY";    else
        if ((tdata->routes[leg->route].attributes & m_rail)      == m_rail)      mode = "RAIL";      else
        if ((tdata->routes[leg->route].attributes & m_bus)       == m_bus)       mode = "BUS";       else
        if ((tdata->routes[leg->route].attributes & m_ferry)     == m_ferry)     mode = "FERRY";     else
        if ((tdata->routes[leg->route].attributes & m_cablecar)  == m_cablecar)  mode = "CABLE_CAR"; else
        if ((tdata->routes[leg->route].attributes & m_gondola)   == m_gondola)   mode = "GONDOLA";   else
        if ((tdata->routes[leg->route].attributes & m_funicular) == m_funicular) mode = "FUNICULAR"; else
        mode = "INVALID";
    }
    json_obj(); /* one leg */
        json_place("from", leg->t0, leg->t0, leg->s0, tdata); // TODO We should have stop arrival/departure here
        json_place("to",   leg->t1, leg->t1, leg->s1, tdata); // TODO
        json_kv("legGeometry", NULL);
        json_kv("mode", mode);
        json_kl("startTime", rtime_to_msec(leg->t0));
        json_kl("endTime",   rtime_to_msec(leg->t1));
        json_kl("departureDelay", 0);
        json_kl("arrivalDelay", 0);
        json_kl("departureDelay", 0);
        json_kv("headsign", route_id);
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
    json_end_obj();
}

static void json_itinerary (struct itinerary *itin, tdata_t *tdata) {
    json_obj(); /* one itinerary */
        json_kd("duration", 5190000);
        json_kl("startTime", 1376896831000);
        json_kl("endTime", 1376902021000);
        json_kd("walkTime", 1491);
        json_kd("transitTime", 3267);
        json_kd("waitingTime", 432);
        json_kd("walkDistance", 1887);
        json_kb("walkLimitExceeded",true);
        json_kd("elevationLost",0);
        json_kd("elevationGained",0);
        json_kd("transfers", itin->n_legs / 2 - 1);
        json_key_arr("legs");
            for (int l = 0; l < itin->n_legs; ++l) json_leg (itin->legs + l, tdata);
        json_end_arr();    
    json_end_obj();
}

uint32_t render_plan_json(struct plan *plan, tdata_t *tdata, router_request_t *req, char *buf, uint32_t buflen) {
    struct tm ltm;
    time_t seconds = req_to_epoch(& plan->req, tdata, &ltm);
    char date[11];
    strftime(date, 30, "%Y-%m-%d\0", &ltm);

    json_begin(buf, buflen);
    json_obj();
        json_kv("error", "null");
        json_key_obj("requestParameters");
            json_kv("time", timetext(req->time));
            json_kb("arriveBy", plan->req.arrive_by);
            json_kf("maxWalkDistance", 2000.0);
            json_kv("fromPlace", tdata_stop_desc_for_index(tdata, plan->req.from));
            json_kv("toPlace",   tdata_stop_desc_for_index(tdata, plan->req.to));
            json_kv("date", date);
            if (plan->req.mode == m_all) {
                json_kv("mode", "TRANSIT,WALK");
            } else {
                char modes[67]; // max length is 58 + 4 + 8 = 70, minus shortest (3 + 1) + 1
                char *dst = modes;

                if ((plan->req.mode & m_tram)      == m_tram)      dst = strcpy(dst, "TRAM,");
                if ((plan->req.mode & m_subway)    == m_subway)    dst = strcpy(dst, "SUBWAY,");
                if ((plan->req.mode & m_rail)      == m_rail)      dst = strcpy(dst, "RAIL,");
                if ((plan->req.mode & m_bus)       == m_bus)       dst = strcpy(dst, "BUS,");
                if ((plan->req.mode & m_ferry)     == m_ferry)     dst = strcpy(dst, "FERRY,");
                if ((plan->req.mode & m_cablecar)  == m_cablecar)  dst = strcpy(dst, "CABLE_CAR,");
                if ((plan->req.mode & m_gondola)   == m_gondola)   dst = strcpy(dst, "GONDOLA,");
                if ((plan->req.mode & m_funicular) == m_funicular) dst = strcpy(dst, "FUNICULAR,");

                dst = strcpy(dst, "WALK");

                json_kv("mode", modes);
            }
        json_end_obj();
        json_key_obj("plan");
            json_kl("date", seconds);
            json_place("from", UNREACHED, UNREACHED, plan->req.from, tdata);
            json_place("to", UNREACHED, UNREACHED, plan->req.to, tdata);
            json_key_arr("itineraries");
                for (int i = 0; i < plan->n_itineraries; ++i) json_itinerary (plan->itineraries + i, tdata);
            json_end_arr();    
        json_end_obj();
        #if 0
        json_key_obj("debug");
            json_kd("precalculationTime", 12);
            json_kd("pathCalculationTime", 808);
            json_kb("timedOut", false);
        json_end_obj();
        #endif
    json_end_obj();
    // json_dump();
    return json_length();
}

uint32_t json_result_dump(router_t *router, router_request_t *req, char *buf, uint32_t buflen) {
    struct plan plan;
    router_result_to_plan (&plan, router, req);
    return render_plan_json (&plan, &(router->tdata), req, buf, buflen);
}
