ENABLE_TESTING()
find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CHECK_LIBRARIES})
include_directories(. ..)

set(SOURCE_FILES
    ../bitset.c
    ../bitset.h
    ../geometry.c
    ../geometry.h
    ../hashgrid.c
    ../hashgrid.h
    ../json.c
    ../json.h
    ../polyline.c
    ../polyline.h
    ../radixtree.c
    ../radixtree.h
    ../router_request.c
    ../router_request.h
    ../street_network.c
    ../street_network.h
    ../tdata.c
    ../tdata.h
    ../gtfs-realtime.pb-c.c
    ../gtfs-realtime.pb-c.h
    ../tdata_realtime_alerts.c
    ../tdata_realtime_alerts.h
    ../tdata_realtime_expanded.c
    ../tdata_realtime_expanded.h
    ../tdata_validation.c
    ../tdata_validation.h
    ../tdata_io_v4_dynamic.c
    ../tdata_io_v4.h
    ../index_journey_pattern_points.c
    ../index_journey_pattern_points.h
    ../index_journey_patterns.c
    ../index_journey_patterns.h
    ../index_vehicle_journeys.c
    ../index_vehicle_journeys.h
    ../tdata_jit.c
    ../tdata_jit.h
    ../string_pool.c
    ../string_pool.h
    ../util.c
    ../util.h
    run_tests.c
    test_bitset.c
    test_hashgrid.c
    test_geometry.c
    test_json.c
    test_radixtree.c
    test_polyline.c
    test_router_request.c
    test_street_network.c
    test_util.c
    test_tdata_jit.c
    )

add_executable(tests ${SOURCE_FILES})
SET_TARGET_PROPERTIES(tests PROPERTIES
  COMPILE_FLAGS "-DRRRR_DEBUG ${SHARED_FLAGS}"
)
target_link_libraries(tests ${LIBS} pthread)
add_test(tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
