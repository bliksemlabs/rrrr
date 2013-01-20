#include "craptor.h"

#define BUFLEN 255

/* parse a cgi query string returning one key-value pair at a time */
inline boolean next_query_param(const char *qstring, char *buf, char **vbuf, int buflen) {
    static const char *q = NULL;
    // set up if not currently working on a qstring
    if (q == NULL) 
        q = qstring; 
    
    if (*q == '\0') { 
        q = NULL; // set internal state (no work in progress)
        return FALSE; // signal this query string is fully parsed
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
    return TRUE;
}

/* http://www.geekhideout.com/urlcode.shtml */

/* Converts a hex character to its integer value */
char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* destructively decodes a url-encoded string (the result length is always <= the input length) */
void url_decode (char *buf) {
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

boolean parse_query_params(request_t *req) {
    char *qstring = getenv("QUERY_STRING");
    if (qstring == NULL)
        return FALSE;
    char key[BUFLEN];
    char *val;
    // set defaults
    req->from = req->to = req->time = -1; 
    req->arrive_by = FALSE;
    while (next_query_param(qstring, key, &val, BUFLEN)) {
        url_decode (key);
        url_decode (val);
        if (strcmp(key, "time") == 0) {
            req->time = atoi(val);
        } else if (strcmp(key, "from") == 0) {
            req->from = atoi(val);
        } else if (strcmp(key, "to") == 0) {
            req->to = atoi(val);
        } else {
            printf("unrecognized parameter: key=%s val=%s\n", key, val);
        }
    }
    return TRUE;
}


