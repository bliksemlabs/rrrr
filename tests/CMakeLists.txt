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
    ../json.c
    ../json.h
    ../polyline.c
    ../polyline.h
    ../util.c
    ../util.h
    run_tests.c
    test_bitset.c
    test_geometry.c
    test_json.c
    test_polyline.c
    test_util.c
    #test_hashgrid.c
    #test_radixtree.c
    )

add_executable(tests ${SOURCE_FILES})
SET_TARGET_PROPERTIES(tests PROPERTIES
  COMPILE_FLAGS "-DRRRR_DEBUG ${SHARED_FLAGS}"
)
target_link_libraries(tests ${LIBS} pthread)
add_test(tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
