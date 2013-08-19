/* json.c */

#include "json.h"
#include "geometry.h"

#include <stdio.h>
#include <string.h>

#define BUFLEN (1024 * 32)

static bool in_list = false;
static char buf[BUFLEN];
static char *buf_end = buf + BUFLEN - 1; // -1 to allow for final terminator char
static char *b = buf;
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

static void json_begin() { 
    b = buf; 
    overflowed = false; 
}

static void json_dump() { 
    *b = '\0'; 
    if (overflowed) printf ("[JSON OVERFLOW]\n");
    printf("%s\n", buf); 
}

static size_t json_length() { return b - buf; }

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

static void json_place (char *key, uint32_t stop_index, tdata_t *tdata) {
    char *stop_id = tdata_stop_id_for_index(tdata, stop_index);
    latlon_t coords = tdata->stop_coords[stop_index];
    json_key_obj(key);
        json_kv("name", stop_id);
        json_key_obj("stopId");
            json_kv("agencyId", "NL");
            json_kv("id", stop_id);
        json_end_obj();
        json_kv("stopCode", stop_id);
        json_kv("platformCode", NULL);
        json_kf("lat", coords.lat);
        json_kf("lon", coords.lon);
        json_kd("arrival", 0);
        json_kd("departure", 10);
    json_end_obj();
}

static void json_leg (struct leg *leg, tdata_t *tdata) {
    json_obj(); /* one leg */
        json_place("from", leg->s0, tdata);
        json_place("to",   leg->s1, tdata);
        json_kv("legGeometry", NULL);
/* 
    "startTime": 1376897760000,
    "endTime": 1376898480000,
    "departureDelay": 0,
    "arrivalDelay": 0,
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
        json_kd("transfers", 2);
        json_key_arr("legs");
            for (int l = 0; l < itin->n_legs; ++l) json_leg (itin->legs + l, tdata);
        json_end_arr();    
    json_end_obj();
}

void render_plan_json(struct plan *plan, tdata_t *tdata) {
    json_begin();
    json_obj();
        json_kv("error", "null");
        json_key_obj("requestParameters");
            json_kv("time", timetext(SEC_TO_RTIME(plan->req.time)));
            json_kb("arriveBy", plan->req.arrive_by);
            json_kf("maxWalkDistance", 1500.0);
            json_kv("fromPlace", tdata_stop_id_for_index(tdata, plan->req.from));
            json_kv("toPlace",   tdata_stop_id_for_index(tdata, plan->req.to));
            json_kv("date", "08-19-2013");
            json_kv("mode", "TRANSIT,WALK");
        json_end_obj();
        json_key_obj("plan");
            json_kl("date", 1376896320000);
            json_place("from", plan->req.from, tdata);
            json_place("to", plan->req.to, tdata);
            json_key_arr("itineraries");
                for (int i = 0; i < plan->n_itineraries; ++i) json_itinerary (plan->itineraries + i, tdata);
            json_end_arr();    
        json_end_obj();
        json_key_obj("debug");
            json_kd("precalculationTime", 12);
            json_kd("pathCalculationTime", 808);
            json_kb("timedOut", false);
        json_end_obj();
    json_end_obj();
    json_dump();
}

