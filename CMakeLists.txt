cmake_minimum_required(VERSION 2.8.4)
project(ansi)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wall -ansi -pedantic")

SET(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/gtfs-realtime.proto
)

SET(PROTO_HDRS
    ${CMAKE_CURRENT_SOURCE_DIR}/gtfs-realtime.pb-c.h
)

SET(PROTO_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/gtfs-realtime.pb-c.c
)

add_custom_command(OUTPUT ${PROTO_HDRS} ${PROTO_SRCS}
    COMMAND protoc-c ARGS ${PROTO_FILES} -I ${CMAKE_CURRENT_SOURCE_DIR}
    --c_out=${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/." VERBATIM
    DEPENDS ${PROTO_FILES})

ENABLE_TESTING()

set(SOURCE_FILES
    bitset.c
    bitset.h
    cli.c
    config.h
    geometry.c
    geometry.h
    gtfs-realtime.pb-c.c
    gtfs-realtime.pb-c.h
    hashgrid.c
    hashgrid.h
    list.c
    list.h
    radixtree.c
    radixtree.h
    router.c
    router.h
    router_dump.c
    router_dump.h
    router_request.c
    router_request.h
    router_result.c
    router_result.h
    rrrr_types.h
    tdata.c
    tdata.h
    tdata_io_v3.h
    tdata_io_v3_dynamic.c
    tdata_io_v3_mmap.c
    tdata_realtime_alerts.c
    tdata_realtime_alerts.h
    tdata_realtime_expanded.c
    tdata_realtime_expanded.h
    tdata_validation.c
    tdata_validation.h
    util.c
    util.h)

link_libraries(m)
link_libraries(protobuf-c)

add_executable(cli ${SOURCE_FILES})

add_subdirectory(tests)
