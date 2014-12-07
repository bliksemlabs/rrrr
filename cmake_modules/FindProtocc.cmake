if(PROTOCC_EXECUTABLE)
    # be silent if already found
    set(PROTOCC_FIND_QUIETLY TRUE)
endif()

find_program(PROTOCC_EXECUTABLE
    NAMES	protoc-c
    PATHS	/usr/bin /usr/local/bin
    DOC		"protobuf-c compiler")

if(PROTOCC_EXECUTABLE)
    set(PROTOCC_FOUND TRUE)
    if(NOT PROTOCC_FIND_QUIETLY)
        message(STATUS "Found protocc: ${PROTOCC_EXECUTABLE}")
    endif()
else()
    message(FATAL_ERROR "Could not find protocc executable")
endif()