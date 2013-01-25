/* util.c : various utility functions */
#include "util.h"
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

void die(const char *msg) {
    syslog(LOG_ERR, "%s", msg);
    exit(EXIT_FAILURE);
}

static char buf[32];

char *timetext(int t) {
    if (t == INT_MAX) {
        return "   --   ";
    }        
    int s = t % 60;
    int m = t / 60;
    int h = m / 60;
    m = m % 60;
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    return buf;
}
