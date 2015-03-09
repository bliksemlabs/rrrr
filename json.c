/* Copyright 2013–2015 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and
 * at https://github.com/bliksemlabs/rrrr/
 */

/* json.c : helper functions to build a JSON document */

#include "json.h"
#include <inttypes.h>

/* private functions */

/* Check an operation that will write multiple characters to the buffer,
 * when the maximum number of characters is known.
 */
static bool remaining(json_t *j, size_t n) {
    if (j->b + n < j->buf_end) return true;
    j->overflowed = true;
    return false;
}

/* Overflow-checked copy of a single char to the buffer.
 */
static void check(json_t *j, char c) {
    if (j->b >= j->buf_end) j->overflowed = true;
    else *(j->b++) = c;
}

/* Add a comma to the buffer, but only if we are currently in a list.
 */
static void comma(json_t *j) {
    if (j->in_list) check(j, ',');
}

/* Write a string out to the buffer, surrounding it in quotes and
 * escaping all quotes or slashes.
 */
static void string (json_t *j, const char *s) {
    const char *c;

    if (s == NULL) {
        if (remaining(j, 4)) j->b += sprintf(j->b, "null");
        return;
    }
    check(j, '"');
    for (c = s; *c != '\0'; ++c) {
        switch (*c) {
        case '\\' :
        case '\b' :
        case '\f' :
        case '\n' :
        case '\r' :
        case '\t' :
        case '\v' :
        case '"' :
            check(j, '\\');
        default:
            check(j, *c);
        }
    }
    check(j, '"');
}

/* Escape a key and copy it to the buffer, preparing for a single value.
 * This should only be used internally, since it sets in_list _before_
 * the value is added.
 */
static void ekey (json_t *j, const char *k) {
    comma(j);
    if (k) {
        string(j, k);
        check(j, ':');
    }
    j->in_list = true;
}


/* public functions (eventually) */

/* Initialise the struct to store the JSON document.
 */
void json_init (json_t *j, char *buf, size_t buflen) {
    j->buf_start = j->b = buf;
    j->buf_end = j->b + buflen - 1;
    j->in_list = false;
    j->overflowed = false;
}

/* Finish the JSON document
 */
void json_end (json_t *j) {
    *j->b = '\0';
}

/* Add a string value to the JSON document.
 */
void json_kv(json_t *j, const char *key, const char *value) {
    ekey(j, key);
    string(j, value);
}

/* Add a signed integer value to the JSON document.
 */
void json_kd(json_t *j, const char *key, int value) {
    ekey(j, key);
    if (remaining(j, 11)) j->b += sprintf(j->b, "%d", value);
}

/* Add a floating point value to the JSON document.
 */
void json_kf(json_t *j, const char *key, double value) {
    ekey(j, key);
    if (remaining(j, 12)) j->b += sprintf(j->b, "%5.5f", value);
}

/* Add a long signed integer to the JSON document.
 */
void json_kl(json_t *j, const char *key, int64_t value) {
    ekey(j, key);
    if (remaining(j, 21)) j->b += sprintf(j->b, "%" PRId64 , value);
}

/* Add a boolean value to the JSON document.
 */
void json_kb(json_t *j, const char *key, bool value) {
    ekey(j, key);
    if (remaining(j, 5)) j->b += sprintf(j->b, value ? "true" : "false");
}

/* Start a new object inside the JSON document.
 */
void json_key_obj(json_t *j, const char *key) {
    ekey(j, key);
    check(j, '{');
    j->in_list = false;
}

/* Start a new array inside the JSON document.
 */
void json_key_arr(json_t *j, const char *key) {
    ekey(j, key);
    check(j, '[');
    j->in_list = false;
}

/* Close an object in the JSON document.
 */
void json_end_obj(json_t *j) {
    check(j, '}');
    j->in_list = true;
}

/* Close an array in the JSON document.
 */
void json_end_arr(json_t *j) {
    check(j, ']');
    j->in_list = true;
}

/* Return the length in bytes of the JSON document.
 */
size_t json_length(json_t *j) {
    return (size_t) (j->b - j->buf_start);
}

#ifdef RRRR_DEBUG
/* Dump the current JSON document for debugging
 */
void json_dump(json_t *j) {
    *j->b = '\0';
    if (j->overflowed) fprintf (stderr, "[JSON OVERFLOW]\n");
    fprintf(stderr, "%s\n", j->buf_start);
}
#endif
