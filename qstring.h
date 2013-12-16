/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* qstring.h */
#include <stdbool.h>
#include <stdint.h>

bool qstring_next_pair(const char *qstring, char *buf, char **vbuf, uint32_t buflen);

