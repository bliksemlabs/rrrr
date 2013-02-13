/* iter.c : iterators */
#include "iter.h"
#include <stdbool.h>

inline void iter_init(iter it, int *ary, int ilo, int ihi) {
    it.lo = ary + ilo;
    it.hi = ary + ihi;
    it.cur = it.lo;
} 

inline bool iter_has_next(iter it) {
    return it.cur < it.hi;
}

inline int iter_index(iter it) {
    return it.cur - it.lo;
}

inline int iter_value(iter it) {
    return *(it.cur);
}

inline int iter_next(iter it) {
    return *((it.cur)++);
}

inline void iter_reset(iter it) {
    it.cur = it.lo;
}

