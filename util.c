/* util.c : various utility functions */
#include "util.h"
#include <syslog.h>
#include <stdlib.h>

void die(const char *msg) {
    syslog(LOG_ERR, "%s", msg);
    exit(EXIT_FAILURE);
}

