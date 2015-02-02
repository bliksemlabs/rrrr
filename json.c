/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

/* json.c */

#include "json.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

/* private functions */

/* Check an operation that will write multiple characters to the buffer,
 * when the maximum number of characters is known.
 */
static bool remaining(json_t *j, size_t n) {
    if (j->b + n < j->buf_end) return true;
    j->overflowed = true;
    return false;
}

/* Overflow-checked copy of a single char to the buffer. */
static void check(json_t *j, char c) {
    if (j->b >= j->buf_end) j->overflowed = true;
    else *(j->b++) = c;
}

/* Add a comma to the buffer, but only if we are currently in a list. */
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
    string(j, k);
    check(j, ':');
    j->in_list = true;
}

/* public functions (eventually) */

void json_init (json_t *j, char *buf, size_t buflen) {
    j->buf_start = j->b = buf;
    j->buf_end = j->b + buflen - 1;
    j->in_list = false;
    j->overflowed = false;
}

void json_kv(json_t *j, char *key, const char *value) {
    ekey(j, key);
    string(j, value);
}

void json_kd(json_t *j, char *key, int value) {
    ekey(j, key);
    if (remaining(j, 11)) j->b += sprintf(j->b, "%d", value);
}

void json_kf(json_t *j, char *key, double value) {
    ekey(j, key);
    if (remaining(j, 12)) j->b += sprintf(j->b, "%5.5f", value);
}

void json_kl(json_t *j, char *key, int64_t value) {
    ekey(j, key);
    if (remaining(j, 21)) j->b += sprintf(j->b, "%" PRId64 , value);
}

void json_kb(json_t *j, char *key, bool value) {
    ekey(j, key);
    if (remaining(j, 5)) j->b += sprintf(j->b, value ? "true" : "false");
}

void json_key_obj(json_t *j, char *key) {
    if (key)
        ekey(j, key);
    else
        comma(j);
    check(j, '{');
    j->in_list = false;
}

void json_key_arr(json_t *j, char *key) {
    ekey(j, key);
    check(j, '[');
    j->in_list = false;
}

void json_obj(json_t *j) {
    comma(j );
    check(j, '{');
    j->in_list = false;
}

void json_arr(json_t *j) {
    comma(j);
    check(j, '[');
    j->in_list = false;
}

void json_end_obj(json_t *j) {
    check(j, '}');
    j->in_list = true;
}

void json_end_arr(json_t *j) {
    check(j, ']');
    j->in_list = true;
}

size_t json_length(json_t *j) {
    return j->b - j->buf_start;
}

void json_dump(json_t *j) {
    *j->b = '\0';
    if (j->overflowed) printf ("[JSON OVERFLOW]\n");
    printf("%s\n", j->buf_start);
}
