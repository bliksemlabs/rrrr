/* util.c : various utility functions */
#include "util.h"

#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

void die(const char *msg) {
    syslog(LOG_ERR, "%s", msg);
    exit(EXIT_FAILURE);
}

static char buf[32];

// buffer should always be at least 9 characters long, including terminating null
char *btimetext(rtime_t rt, char *buf) {
    if (rt == UNREACHED) {
        strcpy(buf, "   --   ");
        return buf;
    }
    uint32_t t = RTIME_TO_SEC(rt);
    uint32_t s = t % 60;
    uint32_t m = t / 60;
    uint32_t h = m / 60;
    m = m % 60;
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    return buf;
}

char *timetext(rtime_t t) {
    return btimetext(t, buf);
}

//assumes little endian http://stackoverflow.com/a/3974138/778449
// size in bytes
void printBits(size_t const size, void const * const ptr) {
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    uint32_t i, j;
    for (i=size-1;i>=0;i--) {
        for (j=7;j>=0;j--) {
            byte = b[i] & (1<<j);
            byte >>= j;
            printf("%u", byte);
        }
    }
    puts("");
}

