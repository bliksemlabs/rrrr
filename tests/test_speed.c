#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#include "../hashgrid.h"
#include "../tdata.h"
#include "../router.h"
#include "../config.h"

#define N_REQUESTS 100

static struct timeval t0;
static long *response_times = NULL;
static int   n_responses;

static void stats_init (int n) {
    response_times = malloc (sizeof(*response_times) * n);
    n_responses = 0;
}

static inline void stats_begin_clock () {
    gettimeofday(&t0, NULL);
}

static inline long stats_end_clock () {
    struct timeval t1;
    gettimeofday(&t1, NULL);
    long dt = ((t1.tv_usec + 1000000 * t1.tv_sec) - (t0.tv_usec + 1000000 * t0.tv_sec));    
    return dt;
}

static inline void stats_end_record () {
    response_times[n_responses++] = stats_end_clock ();
}

static void stats_calculate () {
    double min =  INFINITY;
    double max = -INFINITY;
    double avg =  0;
    for (int i = 0; i < n_responses; ++i) {
        double sec = response_times[i] / 1000000.0;
        if (sec > max) max = sec;
        if (sec < min) min = sec;
        avg += sec;
    }
    avg /= n_responses;
    printf ("min %0.04f sec, max %0.04f sec, avg %0.04f sec\n", min, max, avg);
    ck_assert_msg (min > 0.0005, "Minimum response time was suspect (almost 0).");
    ck_assert_msg (max < 1.0,    "Maximum response time was too high ( >1 sec).");
    ck_assert_msg (avg < 0.1,    "Average response time was too high.");    
    free (response_times);
}

START_TEST (test_speed_random) {
    tdata_t tdata;
    tdata_load (RRRR_INPUT_FILE, &tdata);
    router_t router;
    router_setup (&router, &tdata);
    // ensure different random requests on different runs
    srand(time(NULL)); 
    stats_init (N_REQUESTS);
    router_request_t req;
    router_request_initialize (&req);
    printf ("random requests.");
    for (int i = 0; i < N_REQUESTS; ++i) {
        printf (".");
        fflush (stdout);
        router_request_randomize (&req, &tdata);
        stats_begin_clock ();
        router_route (&router, &req);
        stats_end_record ();
    }
    printf ("\n");
    stats_calculate (); 
} END_TEST

START_TEST (test_speed_mmri) {

} END_TEST

Suite *make_speed_suite (void) {
    Suite *s = suite_create ("Speed");
    TCase *tc_rand = tcase_create ("Random");
    tcase_add_test (tc_rand, test_speed_random);
    tcase_set_timeout (tc_rand, 15);
    suite_add_tcase (s, tc_rand);
//    TCase *tc_mmri = tcase_create ("MMRI");
//    tcase_add_test  (tc_mmri, test_speed_mmri);
//    suite_add_tcase (s, tc_mmri);
    return s;
}

