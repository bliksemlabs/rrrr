#ifndef _CONFIG_H
#define _CONFIG_H

/* These are the default options for a router_request */

/* Maximum iterations the algorithm will run */
#define RRRR_DEFAULT_MAX_ROUNDS 6

/* Walk slack in seconds */
#define RRRR_DEFAULT_WALK_SLACK 0

/* Speed by foot, in meter per second */
#define RRRR_DEFAULT_WALK_SPEED 1.5

/* Maximum distance in meters to travel by feet from the
 * origin to the first stop_point, and from the last stop_point to
 * the destination.
 */
#define RRRR_DEFAULT_WALK_MAX_DISTANCE 500

#define RRRR_MAX_BANNED_JOURNEY_PATTERNS 1
#define RRRR_MAX_BANNED_STOP_POINTS 1
#define RRRR_MAX_BANNED_STOP_POINTS_HARD 1
#define RRRR_MAX_BANNED_VEHICLE_JOURNEYS 1

#define RRRR_FEATURE_LATLON 1

#define RRRR_WALK_COMP 1.2

#define RRRR_BANNED_JOURNEY_PATTERNS_BITMASK 0

#if RRRR_MAX_BANNED_JOURNEY_PATTERNS == 0
#undef RRRR_BANNED_JOURNEY_PATTERNS_BITMASK
#endif

#if (defined(RRRR_TDATA_IO_MMAP) && defined(RRRR_TDATA_IO_DYNAMIC)) || (!defined(RRRR_TDATA_IO_MMAP) && !defined(RRRR_TDATA_IO_DYNAMIC))
#define RRRR_TDATA_IO_DYNAMIC 1
#endif

#ifndef RRRR_TDATA_IO_MMAP
#define RRRR_FEATURE_REALTIME_EXPANDED 1
#define RRRR_FEATURE_REALTIME_ALERTS 1
#define RRRR_FEATURE_REALTIME 1

#define RRRR_DYNAMIC_SLACK 2
#endif

/* roughly the length of common prefixes in IDs */
#define RRRR_RADIXTREE_PREFIX_SIZE 4

/* with prefix size of 4 and -m32, edge size is 16 bytes, total 11.9MB
 * with prefix size of 4 and -m64, edge size is 24 bytes, total 17.8MB
 * total size of all ids is 15.6 MB
 * could use int indexes into a fixed-size, pre-allocated edge pool.
 */

#endif
