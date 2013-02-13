/* iter.h */
#include <stdbool.h>

typedef struct iter iter;
struct iter {
    int *lo;
    int *cur;
    int *hi;
};

inline bool iter_has_next(iter);

inline int iter_index(iter);

inline int iter_value(iter);

inline int iter_next(iter);

inline void iter_reset(iter);
