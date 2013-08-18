/* iter.c : iterators */
#include "iter.h"
#include <stdbool.h>

inline void iter_init(iter it, uint32_t *ary, uint32_t ilo, uint32_t ihi) {
    it.lo = ary + ilo;
    it.hi = ary + ihi;
    it.cur = it.lo;
} 

inline bool iter_has_next(iter it) {
    return it.cur < it.hi;
}

inline uint32_t iter_index(iter it) {
    return it.cur - it.lo;
}

inline uint32_t iter_value(iter it) {
    return *(it.cur);
}

inline uint32_t iter_next(iter it) {
    return *((it.cur)++);
}

inline void iter_reset(iter it) {
    it.cur = it.lo;
}

