/* config.h */

#define RRRR_TEST_CONCURRENCY 4
#define RRRR_TEST_REQUESTS 500
#define RRRR_INPUT_FILE "./timetable.dat"
#define RRRR_MAX_ROUNDS 6

// bind does not work with names (localhost) but does work with *
#define CLIENT_ENDPOINT "tcp://127.0.0.1:9292"
#define WORKER_ENDPOINT "tcp://127.0.0.1:9293"

