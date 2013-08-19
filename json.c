/* json.c */

#include "json.h"
#include <stdio.h>

static bool json_comma = false;
static char json_buf[30000];
static char *jb = json_buf;

static void json_begin() { 
    jb = json_buf; 
}

static void json_dump() { 
    printf("%s\n", json_buf);
}

static void json_kv(char *key, char *value) {
    jb += sprintf(jb, "%s\"%s\":\"%s\"", json_comma ? "," : "", key, value);
    json_comma = true;
}

static void json_kd(char *key, int value) {
    jb += sprintf(jb, "%s\"%s\":%d", json_comma ? "," : "", key, value);
    json_comma = true;
}

static void json_kl(char *key, long value) {
    jb += sprintf(jb, "%s\"%s\":%ld", json_comma ? "," : "", key, value);
    json_comma = true;
}

static void json_kb(char *key, bool value) {
    jb += sprintf(jb, "%s\"%s\":%s", json_comma ? "," : "", key, value ? "true" : "false");
    json_comma = true;
}

static void json_key_obj(char *key) {
    jb += sprintf(jb, "%s\"%s\":{", json_comma ? "," : "", key);
    json_comma = false;
}

static void json_key_arr(char *key) {
    jb += sprintf(jb, "%s\"%s\":[", json_comma ? "," : "", key);
    json_comma = false;
}

static void json_obj() {
    jb += sprintf (jb, "{");
    json_comma = false;
}

static void json_end_obj() {
    jb += sprintf (jb, "}");
    json_comma = true;
}

static void json_end_arr() {
    jb += sprintf (jb, "]");
    json_comma = true;
}


void render_plan_json(struct plan *plan, tdata_t *tdata) {
    
    json_begin();
    json_obj();
        json_kv("error", "null");
        json_key_obj("requestParameters");
            json_kv("time", "9:12am");
            json_kv("arriveBy", "false");
            json_kv("maxWalkDistance", "1500");
            json_kv("fromPlace", "51.93749209045435,4.51263427734375");
            json_kv("toPlace", "52.36469901960148,4.9053955078125");
            json_kv("date", "08-19-2013");
            json_kv("mode", "TRANSIT,WALK");
        json_end_obj();
        json_key_obj("plan");
            json_kl("date", 1376896320000);
            json_key_obj("from");
                json_kv("name", "Langepad");
                json_kd("stopId", 0);
                json_kd("stopCode", 0);
                json_kd("platformCode", 0);
                json_kd("lon", 4.50870);
                json_kd("lat", 51.9364);
            json_end_obj();
            json_key_obj("to");
                json_kv("name", "Weesperstraat");
                json_kd("lon", 4.9057266219348);
                json_kd("lat", 52.364778630779);
            json_end_obj();
            json_key_arr("itineraries");
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
                        json_obj(); /* one leg */
                        
/* A LEG 
  {
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
  }                        
*/                        
                        json_end_obj();
                    json_end_arr();    
                json_end_obj();
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

