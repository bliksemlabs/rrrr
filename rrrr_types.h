#ifndef _RRRR_TYPES_H
#define _RRRR_TYPES_H

typedef uint16_t rtime_t;

#ifndef _LP64
    #define ZU "%u"
#else
    #define ZU "%lu"
#endif

#endif /* _RRRR_TYPES */
