#include <stdio.h>
#include <stdint.h>
#include "container.h"

void route_type_txt (tdata_container_t *container) {
    const char *id[] = { "0", "1", "2", "3", "4", "5", "6", "7" };
    const char *name[] = { "Tram", "Metro", "Rail", "Bus", "Ferry", "Cable car", "Gondola", "Funicular" };
    tdata_physical_modes_add (&container->pms, id, name, 8, NULL);
    tdata_commercial_modes_add (&container->cms, id, name, 8, NULL);
}

void agency_txt (tdata_container_t *container) {
    const char *agency_id = "1313";
    const char *agency_url = "http://1313.nl";
    const char *agency_name = "Bliksem Labs B.V.";
    tdata_operators_add (&container->ops, &agency_id, &agency_url, &agency_name, 1, NULL);
}

void stops_txt (tdata_container_t *container) {
    const char *stop_id[] = {"1a1", "1a2", "1a3", "1a4", "1a5", "1a6"};
    const char *stop_name[] = {"Stop 1a1", "Stop 1a2", "Stop 1a3", "Stop 1a4", "Stop 1a5", "Stop 1a6"};
    const latlon_t ll[] = {{1.101, 1.1}, {1.102, 1.1}, {1.103, 1.1}, {1.104, 1.1}, {1.105, 1.1}, {1.106, 1.1}};

    /* TODO: figure out if it is reasonable to push NULL */
    const char *platform_code[] = {"", "", "", "", "", ""};
    const spidx_t parent_station[] = {-1, -1, -1, -1, -1, -1};
    const uint8_t attributes[] = {0, 0, 0, 0, 0, 0};
    const rtime_t wait_time[] = {0, 0, 0, 0, 0, 0};

    tdata_stop_points_add (&container->sps, stop_id, stop_name, platform_code, ll, wait_time, parent_station, attributes, 6, NULL);
}

void transfers_txt (tdata_container_t *container) {
    /* TODO: lookup the stop_id
    const char *from_stop_id[] = {"1a2", "1a4"};
    const char *to_stop_id[] = {"1a3", "1a5"};
    */
    const spidx_t to_stop_id[] = {2, 4};
    const rtime_t min_transfer_time[] = {0, 0};

    tdata_transfers_add (&container->transfers, to_stop_id, min_transfer_time, 2, NULL);
    container->sps.transfers_offset[1] = 0;
    container->sps.transfers_offset[3] = 1;
}

void routes_txt (tdata_container_t *container) {
    /* TODO: lookup the agency_id */
    const opidx_t agency_id[] = {0, 0, 0};
    const char *route_id[] = {"1a|bus", "1a|ferry", "1a|train" };
    const char *route_short_name[] = { "bus", "ferry", "train" };
    const char *route_long_name[] = { "", "", "" };
    const pmidx_t route_type[] = {3, 4, 2};
    const char *route_color[] = {"AA0000", "00AA00", "0000AA"};
    const char *route_color_text[] = {"FFFFFF", "FFFFFF", "FFFFFF"};

    const lineidx_t line_for_routes[] = { 0, 1, 2 };

    tdata_lines_add (&container->lines,
                     route_id, route_short_name, route_long_name,
                     route_color, route_color_text,
                     agency_id, route_type, 3, NULL);

    tdata_routes_add (&container->routes, line_for_routes, 3, NULL);
}

void stop_times_txt (tdata_container_t *container) {
    const rtime_t arrival_time[] = { 0, 15 };
    const rtime_t departure_time[] = { 0, 15 };

    const spidx_t stop_id[] = { 0, 1, 2, 3, 4, 5 };
    const char *headsign[] = { "", "", "", "", "", "" };
    const uint8_t attributes[] = { 0, 0, 0, 0, 0, 0 };

    tdata_stop_times_add (&container->sts, arrival_time, departure_time, 2, NULL);

    tdata_journey_pattern_points_add (&container->jpps, stop_id, headsign, attributes, 6, NULL);
}

void trips_txt (tdata_container_t *container) {
    const char* trip_id[] = { "1a|bus|1", "1a|ferry|1", "1a|train|1" };
    const uint32_t stop_times_offset[] = {0, 0, 0};
    const rtime_t begin_time[] = { 15, 45, 90 };
    const vj_attribute_mask_t vj_attributes[] = {0, 0, 0};
    const calendar_t vj_active[] = {1, 1, 1};
    const int8_t vj_time_offsets[] = {0, 0, 0};
    const vehicle_journey_ref_t vehicle_journey_transfers_forward[] = {{0, 0}, {0, 0}, {0, 0}};
    const vehicle_journey_ref_t vehicle_journey_transfers_backward[] = {{0, 0}, {0, 0}, {0, 0}};
    const uint32_t vj_transfers_forward_offset[] = {0, 0, 0};
    const uint32_t vj_transfers_backward_offset[] = {0, 0, 0};
    const uint8_t n_transfers_forward[] = {0, 0, 0};
    const uint8_t n_transfers_backward[] = {0, 0, 0};


    const uint32_t journey_pattern_point_offset[] = {0, 2, 4};
    const vjidx_t vj_index[] = {0, 1, 2};
    const jppidx_t n_stops[] = {2, 2, 2};
    const jp_vjoffset_t n_vjs[] = {1, 1, 1};
    const uint16_t attributes[] = {0, 0, 0};
    const routeidx_t route_index[] = {0, 1, 2};
    const cmidx_t commercial_mode_for_jp[] = {0, 1, 2};

    tdata_vehicle_journeys_add (&container->vjs, trip_id, stop_times_offset, begin_time, vj_attributes, vj_active, vj_time_offsets, vehicle_journey_transfers_forward, vehicle_journey_transfers_backward, vj_transfers_forward_offset, vj_transfers_backward_offset, n_transfers_forward, n_transfers_backward, 3, NULL);

    tdata_journey_patterns_add (&container->jps, journey_pattern_point_offset, vj_index, n_stops, n_vjs, attributes, route_index, commercial_mode_for_jp, 3, NULL);
}

int main(int argv, char *args[]) {
    tdata_container_t container;

    (void)(argv);
    (void)(args);

    tdata_container_init (&container);

    route_type_txt (&container);
    agency_txt (&container);
    stops_txt (&container);
    transfers_txt (&container);
    stop_times_txt (&container);
    routes_txt (&container);
    trips_txt (&container);

    tdata_container_mrproper (&container);

    return 0;
}

