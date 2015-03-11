#include <check.h>
#include <stdlib.h>
#include "../radixtree.h"

START_TEST (test_radixtree)
    {
        radixtree_t *r;

        char *strings[] = {"eight thousand",
                           "nine thousand",
                           "nine tho",
                           "eight thousand five hundred",
                           "six thousand",
                           "nine thousand five",
                           "nine thousand five hundred",
                           "nine nine nine nine",
                           NULL};

        /* tests radixtree_new, rxt_init, rxt_edge_new */
        r = radixtree_new();
        ck_assert (r);
        ck_assert (r->base == NULL);
        ck_assert (r->size == 0);
        ck_assert (r->root);
        ck_assert (r->root->next == NULL);
        ck_assert (r->root->child == NULL);
        ck_assert (r->root->value == RADIXTREE_NONE);
        ck_assert (r->root->prefix[0] == '\0');

        radixtree_insert(r, "", 3);
        radixtree_insert(r, "eight thousand", 8000);
        radixtree_insert(r, "nine thousand", 9000);
        radixtree_insert(r, "eight thousand five hundred", 8500);

        ck_assert_int_eq (radixtree_find_exact(r, strings[0]), 8000);
        ck_assert_int_eq (radixtree_find_exact(r, strings[1]), 9000);
        ck_assert_int_eq (radixtree_find_prefix(r, strings[2], NULL), 9000);

        radixtree_destroy (r);
    }
END_TEST

Suite *make_radixtree_suite(void);

Suite *make_radixtree_suite(void) {
    Suite *s = suite_create("radixtree");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_radixtree);
    suite_add_tcase(s, tc_core);
    return s;
}
