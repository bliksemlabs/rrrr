ENABLE_TESTING()
find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CHECK_LIBRARIES})
include_directories(. ..)

set_property(SOURCE ${PROTO_HDRS} ${PROTO_SRCS} PROPERTY GENERATED ON)

set(SOURCE_FILES
    ${PROTO_HDRS}
    ${PROTO_SRCS}
    ../api.c
    ../api.h
    ../bitset.c
    ../bitset.h
    ../config.h
    ../geometry.c
    ../geometry.h
    ../hashgrid.c
    ../hashgrid.h
    ../hashgrid_streetnetwork.c
    ../json.c
    ../json.h
    ../plan.c
    ../plan.h
    ../plan_render_otp.c
    ../plan_render_otp.h
    ../plan_render_text.c
    ../plan_render_text.h
    ../polyline.c
    ../polyline.h
    ../radixtree.c
    ../radixtree.h
    ../router.c
    ../router.h
    ../router_dump.c
    ../router_dump.h
    ../router_result.c
    ../router_result.h
    ../router_request.c
    ../router_request.h
    ../set.c
    ../set.h
    ../street_network.c
    ../street_network.h
    ../tdata.c
    ../tdata.h
    ../tdata_realtime_alerts.c
    ../tdata_realtime_alerts.h
    ../tdata_realtime_expanded.c
    ../tdata_realtime_expanded.h
    ../tdata_validation.c
    ../tdata_validation.h
    ../tdata_io_v4_mmap.c
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
    test_tdata_views.c
    )

add_executable(tests ${SOURCE_FILES})
add_dependencies(tests cli)
SET_TARGET_PROPERTIES(tests PROPERTIES
  COMPILE_FLAGS "-DRRRR_DEBUG ${SHARED_FLAGS}"
)
target_link_libraries(tests ${LIBS} pthread)
add_test(tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
