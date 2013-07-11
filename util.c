/* util.c : various utility functions */
#include "util.h"

#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

void die(const char *msg) {
    syslog(RRRR_ERR, "%s", msg);
    exit(EXIT_FAILURE);
}

static char buf[32];

// buffer should always be at least 9 characters long, including terminating null
char *btimetext(rtime_t rt, char *buf) {
    if (rt == UNREACHED) {
        strcpy(buf, "   --   ");
        return buf;
    }
    int t = (int)rt << 1;
    int s = t % 60;
    int m = t / 60;
    int h = m / 60;
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
    int i, j;
    for (i=size-1;i>=0;i--) {
        for (j=7;j>=0;j--) {
            byte = b[i] & (1<<j);
            byte >>= j;
            printf("%u", byte);
        }
    }
    puts("");
}

