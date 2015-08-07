#include "rrrr_types.h"
#include "tdata.h"

/* Returns the minimum and maximum rtime_t per journey pattern,
 * and the global maximum rtime_t.
 */
bool index_journey_patterns (const tdata_t *td, calendar_t **jp_active,
                             rtime_t **jp_min, rtime_t **jp_max,
                             rtime_t *td_max);

/* Returns the global maximum rtime_t.
 */
bool index_max_time (const tdata_t *td, rtime_t *td_max);
