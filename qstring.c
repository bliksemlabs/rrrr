#include "craptor.h"

#define BUFLEN 255

// use only one buffer? key is at buf, and val pointer is set somewhere in buf

/* parse a cgi query string returning one key-value pair at a time */
inline boolean next_query_param(const char *qstring, char *keybuf, char *valbuf, int buflen) {
    static const char *c = NULL;
    char *k = keybuf;
    char *v = valbuf;
    // could just have a pointer to the current buffer in use, and set that when we hit =
    
    if (c == NULL) { // not currently working on a qstring
        c = qstring; // set up to start parsing a new qstring
    }
    
    if (*c == '\0') { // reached the end of the query string
        c = NULL;
        return FALSE;
    }
        
    boolean inval = FALSE; // track whether we have passed '=' in the KVP
    while ( *c ) {
        if (*c == '&') {
            ++c;
            break;
        }
        if (inval) { 
            // handle value
            *(v++) = *c;
        } else {    
            // handle key
            if (*c == '=') // move from key to value
                inval = TRUE;
            else
                *(k++) = *c;
        }
        ++c;
    }
    *k = *v = '\0';
    return TRUE;
}

/* destructively decodes a url-encoded string (the result length is always <= than the input length) */
void url_decode (char *buf) {
    char *in, *out;
}

request_t parse_query_params(const char *qstring, FCGX_Stream *out) {
    char key[BUFLEN];
    char val[BUFLEN];
    request_t req;
    
    while (next_query_param(qstring, key, val, BUFLEN)) {
        FCGX_FPrintF(out, "key %s val %s<br>\n", key, val);
        if (strcmp(key, "time") == 0) {
            req.time = 0;
        } else if (strcmp(key, "cheese") == 0) {
            req.time = 0;
        } else {
            // unrecognized parameter
        }
    }
    
}
