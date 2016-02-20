#ifndef TDATA_COMMON_H
#define TDATA_COMMON_H

#include <stdint.h>
#include <stdbool.h>

/* 2^16 / 60 / 60 is 18.2 hours at one-second resolution.
 * By right-shifting times one bit, we get 36.4 hours (over 1.5 days)
 * at 2 second resolution. By right-shifting times two bits, we get
 * 72.8 hours (over 3 days) at 4 second resolution. Three days is just enough
 * to model yesterday, today, and tomorrow for overnight searches, and can
 * also represent the longest rail journeys in Europe.
 */
typedef uint16_t rtime_t;

typedef uint16_t spidx_t;

typedef uint8_t opidx_t;

typedef uint8_t pmidx_t;

typedef uint8_t cmidx_t;

typedef uint16_t lineidx_t;

typedef uint16_t routeidx_t;

typedef uint32_t calendar_t;

typedef uint32_t vjidx_t;

typedef uint16_t jpidx_t;

typedef uint16_t jp_vjoffset_t;

typedef uint16_t jppidx_t;


typedef uint16_t vj_attribute_mask_t;
typedef enum vehicle_journey_attributes {
    vja_none                  = 0,
    vja_wheelchair_accessible = 1,
    vja_bike_accepted         = 2,
    vja_visual_announcement   = 4,
    vja_audible_announcement  = 8,
    vja_appropriate_escort    = 16,
    vja_appropriate_signage   = 32,
    vja_school_vehicle        = 64,
    vja_wifi                  = 128,
    vja_toilet                = 256,
    vja_ondemand              = 512
    /* 5 more attributes allowed */
} vehicle_journey_attributes_t;

typedef struct vehicle_journey_ref vehicle_journey_ref_t;
struct vehicle_journey_ref {
    jppidx_t jp_index;
    jp_vjoffset_t vj_offset;
};


typedef struct {
    float lat;
    float lon;
} latlon_t;

#endif /* TDATA_COMMON_H */
