#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "../radixtree.h"

static void check (uint32_t a, uint32_t b) {
    if (a != b) printf ("mismatch: %d != %d \n", a, b);
    ck_assert (a == b);
}

START_TEST (test_radixtree) {
    struct edge *root = rxt_new();
    rxt_insert(root, "", 3);
    rxt_insert(root, "eight thousand", 8000);
    rxt_insert(root, "nine thousand", 9000);
    rxt_insert(root, "eight thousand five hundred", 8500);
    rxt_edge_print(root);
    char *strings[] = {"eight thousand", 
                       "nine thousand", 
                       "nine tho", 
                       "eight thousand five hundred",
                       "six thousand",
                       "nine thousand five",
                       "nine thousand five hundred",
                       "nine nine nine nine",
                       NULL};
    
    for (char **string = strings; *string != NULL; string += 1) {
        printf("%32s, %d\n", *string, rxt_find(root, *string));
    }
    rxt_insert (root, "nine thousand five hundred", 9500);
    rxt_insert (root, "eight thousand five hundred", 8888);
    printf("\n");
    for (char **string = strings; *string != NULL; string += 1) {
        printf("%32s, %d\n", *string, rxt_find (root, *string));
    }
    rxt_insert(root, "nine thousand five", 9005);
    printf("\n");
    rxt_edge_print(root);
    for (char **string = strings; *string != NULL; string += 1) {
        printf("%32s, %d\n", *string, rxt_find (root, *string));
    }
    rxt_insert(root, "nine nine nine nine", 9999);
    printf("\n");
    rxt_edge_print(root);
    for (char **string = strings; *string != NULL; string += 1) {
        printf("%32s, %d\n", *string, rxt_find (root, *string));
    }
    rxt_insert(root, "six", 6);
    rxt_insert(root, "six thousand", 6000);
    printf("\n");
    rxt_edge_print(root);
    for (char **string = strings; *string != NULL; string += 1) {
        printf("%32s, %d\n", *string, rxt_find (root, *string));
    }
    printf("total number of edges: %d\n", rxt_edge_count (root));
    /*
    char *filename = "trips";
    int fd = open(filename, O_RDONLY);
    if (fd == -1) die("could not find input file");
    struct stat st;
    if (stat(filename, &st) == -1) die("could not stat input file");
    char *trip_ids = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    char *trip_ids_end = trip_ids + st.st_size;
    char *tid = trip_ids;
    uint32_t trip_idx = 0;
    printf ("Indexing trip ids...\n");
    while (tid < trip_ids_end) {
        //printf ("inserting %s, %d\n", tid, trip_idx);
        rxt_insert(root, tid, trip_idx);
        tid += strlen(tid) + 1;
        trip_idx += 1;
    }
    printf("total number of edges: %d\n", edge_count(root));
    printf("size of one edge: %ld\n", sizeof(struct edge));
    printf("total size of all edges: %ld\n", edge_count(root) * sizeof(struct edge));
    
    tid = trip_ids;
    trip_idx = 0;
    printf ("Checking trip ids...\n");
    check (rxt_find (root, "abc"), RADIX_TREE_NONE);
    while (tid < trip_ids_end) {
        uint32_t idx = rxt_find (root, tid);
        check (idx, trip_idx);
        tid += strlen(tid) + 1;
        trip_idx += 1;
    }
    check (rxt_find (root, "xyz"), RADIX_TREE_NONE);
    */
} END_TEST

Suite *make_radixtree_suite (void) {
    Suite *s = suite_create ("RadixTree");
    TCase *tc_core = tcase_create ("Core");
    tcase_add_test  (tc_core, test_radixtree);
    suite_add_tcase (s, tc_core);
    return s;
}

