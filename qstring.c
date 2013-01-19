#include "craptor.h"

#define BUFLEN 20

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

/* destructively decodes a url-encoded string (the result length is always <= than the input length) */
void url_decode (char *buf) {
    char *in, *out;
}

request_t parse_query_params(const char *qstring, FCGX_Stream *out) {
    char key[BUFLEN];
    char *val;
    request_t req;
    
    while (next_query_param(qstring, key, &val, BUFLEN)) {
        FCGX_FPrintF(out, "key %s val %s<br>\n", key, val);
        //syslog(LOG_INFO, "key %s val %s<br>\n", key, val);
        if (strcmp(key, "time") == 0) {
            req.time = 0;
        } else if (strcmp(key, "cheese") == 0) {
            req.time = 0;
        } else {
            // unrecognized parameter
        }
    }
    
    return req;
}
