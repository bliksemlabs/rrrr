/* config.h */

#define RRRR_TEST_CONCURRENCY 4
#define RRRR_INPUT_FILE "/home/abyrd/git/rrrr/timetable.dat"
#define RRRR_MAX_ROUNDS 6

// bind does not work with names (localhost) but does work with *
#define CLIENT_ENDPOINT "tcp://127.0.0.1:9292"
#define WORKER_ENDPOINT "tcp://127.0.0.1:9293"

// #define DEBUG
// #define TRACE

/* http://stackoverflow.com/questions/1644868/c-define-macro-for-debug-printing */
#ifdef DEBUG
 #define D 
#else
 #define D for(;0;)
#endif

#ifdef TRACE
 #define T 
#else
 #define T for(;0;)
#endif
