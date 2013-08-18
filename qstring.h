/* qstring.h */
#include <stdbool.h>
#include <stdint.h>

bool qstring_next_pair(const char *qstring, char *buf, char **vbuf, uint32_t buflen);

