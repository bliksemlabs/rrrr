/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* qstring.c : utility functions for handling cgi query strings */
#include "qstring.h" 
#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>

/* converts a hex character to its integer value */
static unsigned char from_hex(unsigned char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* destructively decodes a url-encoded string (the result length is always <= the input length) */
static void url_decode (char *buf) {
    char *in = buf;
    char *out = buf;
    while ( *in ) {
        if (*in == '+') {
            *out = ' ';
        } else if (*in == '%') {
            if (in[1] && in[2]) {
                *out = from_hex(in[1]) << 4 | from_hex(in[2]);
                in += 2;
            }
        } else {
            *out = *in;
        }
        ++out;
        ++in;
    }
    *out = '\0';
}

/* parse a cgi query string returning one key-value pair at a time */
bool qstring_next_pair(const char *qstring, char *buf, char **vbuf, uint32_t buflen) {
    static const char *q = NULL;
    // set up if not currently working on a qstring
    if (q == NULL) 
        q = qstring; 
    
    if (*q == '\0') { 
        q = NULL; // set internal state (no work in progress)
        return false; // signal this query string is fully parsed
    }
    char *eob = buf + buflen - 1;
    *vbuf = buf; // in case there is no '='
    while (*q != '\0') {
        if (buf >= eob) // check for buffer overflow
            break;
        if (*q == '=') {
            *buf++ = '\0';
            *vbuf = buf;
            ++q;
        } else if (*q == '&') {
            ++q;
            break;
        } else {
            *buf++ = *q++; 
        }
    }
    *buf = '\0'; // terminate value string
    url_decode(buf);
    if (*vbuf != buf)
        url_decode(*vbuf);
    return true;
}


