/* config.h */

#define RRRR_TEST_CONCURRENCY 4
#define RRRR_INPUT_FILE "/home/abyrd/git/rrrr/timetable.dat"

// runtime increases roughly linearly with this value
#define RRRR_MAX_ROUNDS 5

// bind does not work with names (localhost) but does work with * (all interfaces)
#define CLIENT_ENDPOINT "tcp://127.0.0.1:9292"
#define WORKER_ENDPOINT "tcp://127.0.0.1:9293"

// use named pipes instead
// #define CLIENT_ENDPOINT "ipc://client_pipe"
// #define WORKER_ENDPOINT "ipc://worker_pipe"

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


