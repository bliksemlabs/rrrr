#ifndef CRAPTOR_H
#define CRAPTOR_H

#include "fcgi_stdio.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define PROGRAM_NAME "craptor"

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
    int from;
    int to;
    long time;
    boolean arrive_by; 
} request_t;

boolean parse_query_params(request_t *req);

void url_decode (char *buf);

#endif // CRAPTOR_H
