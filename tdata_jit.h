#include "config.h"

#ifdef RRRR_TDATA_IO_DYNAMIC
#include "tdata.h"

bool tdata_journey_patterns_at_stop (tdata_t *td);
bool tdata_journey_patterns_index (tdata_t *td);

bool tdata_journey_pattern_points_append (tdata_t *td, spidx_t *journey_pattern_points, uint8_t *journey_pattern_point_attributes, uint32_t *journey_pattern_point_headsigns, jppidx_t n_stops);

bool tdata_string_pool_new (tdata_t *td, uint32_t n);
void tdata_string_pool_free (tdata_t *td);

uint32_t tdata_string_pool_append (tdata_t *tdata, const char *str);

bool tdata_stop_areas_new (tdata_t *td, spidx_t n);
void tdata_stop_areas_free (tdata_t *td);

void tdata_stop_area_append (tdata_t *td,
                             const char *id,
                             const char *name,
                             const char *timezone,
                             const latlon_t *coord);

bool tdata_stop_points_new (tdata_t *td, spidx_t n);
void tdata_stop_points_free (tdata_t *td);

void tdata_stop_point_append (tdata_t *td,
                              const char *id,
                              const char *name,
                              const char *platformcode,
                              const latlon_t *coord,
                              const rtime_t waittime,
                              const spidx_t stop_area,
                              const uint8_t attributes);

bool tdata_operators_new (tdata_t *td, uint8_t n);
void tdata_operators_free (tdata_t *td);
bool tdata_operators_append (tdata_t *td,
                             const char *id,
                             const char *url,
                             const char *name);


bool tdata_physical_modes_new (tdata_t *td, uint16_t n);
void tdata_physical_modes_free (tdata_t *td);
bool tdata_physical_modes_append (tdata_t *td,
                                  const char *id,
                                  const char *name);

bool tdata_lines_new (tdata_t *td, uint32_t n);
void tdata_lines_free (tdata_t *td);
bool tdata_line_append (tdata_t *td,
                        const char *id,
                        const char *code,
                        const char *name,
                        const char *color,
                        const char *color_text,
                        const uint8_t operator,
                        const uint16_t physical_mode);

bool tdata_commercial_modes_new (tdata_t *td, uint16_t n);
void tdata_commercial_modes_free (tdata_t *td);
bool tdata_commercial_modes_append (tdata_t *td,
                                    const char *id,
                                    const char *name);

bool tdata_journey_patterns_new (tdata_t *td, jpidx_t n_jp, jppidx_t n_jpp, uint16_t n_r);
bool tdata_journey_patterns_append (tdata_t *td, uint32_t jpp_offset, vjidx_t vj_index, jppidx_t n_stops, jp_vjoffset_t n_vjs, uint16_t attributes, uint16_t route_index, uint16_t commercial_mode, lineidx_t line);
void tdata_journey_patterns_free (tdata_t *td);

bool tdata_vehicle_journeys_new (tdata_t *td, vjidx_t n);
void tdata_vehicle_journeys_free (tdata_t *td);
bool tdata_vehicle_journey_append (tdata_t *td,
                                   const char *id,
                                   const uint32_t stop_times_offset,
                                   const rtime_t begin_time,
                                   const vj_attribute_mask_t attributes,
                                   const calendar_t active,
                                   const int8_t time_offset,
                                   const vehicle_journey_ref_t transfer_forward,
                                   const vehicle_journey_ref_t transfer_backward);

bool tdata_stop_times_new (tdata_t *td, uint32_t n);
void tdata_stop_times_free (tdata_t *td);
bool tdata_stop_times_append (tdata_t *td, stoptime_t *stop_times, uint32_t n_stop_times);

bool tdata_transfers_new (tdata_t *td, uint32_t n);
bool tdata_transfers_append (tdata_t *td, spidx_t orig, spidx_t *target_stops, rtime_t *durations, uint8_t n_stops);
void tdata_transfers_free (tdata_t *td);

bool tdata_jit_new (tdata_t *td,
                    const char *timezone,
                    uint64_t calendar_start_time,
                    int32_t utc_offset,
                    rtime_t max_time,
                    uint32_t n_days
                    );

void tdata_jit_free (tdata_t *td);

#endif
