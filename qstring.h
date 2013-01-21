/* qstring.h */
#include <stdbool.h>

bool qstring_next_pair(const char *qstring, char *buf, char **vbuf, int buflen);

void qstring_url_decode (char *buf); // destructive
