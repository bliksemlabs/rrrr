#ifndef CRAPTOR_H
#define CRAPTOR_H

#include "fcgiapp.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <stdio.h>

#define PROGRAM_NAME "craptor"
#define EXIT_SUCCESS 0
#define EXIT_FAIL 1

typedef enum {FALSE = 0, TRUE} boolean;

typedef struct {
    int id;
    char *name;
    uint32_t coord;
} stop_t;

typedef struct {
    int id;
    char *name;
    int route_stops_offset;
} route_t;

typedef struct {
    stop_t *from_stop;
    stop_t *to_stop;
    long time;
    int arrive_by; // bool
} request_t;

request_t parse_query_params(const char *qstring, FCGX_Stream *out);

#endif // CRAPTOR_H
