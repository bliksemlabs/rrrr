#include <check.h>
#include <stdlib.h>
#include "../json.h"

START_TEST (test_json)
    {
        json_t json;
        json_t *j = &json;
        char buf[2048];

        json_init(j, buf, 2048);
        json_obj(j);
        json_key_obj(j, "tests");
        json_kv(j, "string", "hello");
        json_kd(j, "integer", -1);
        json_kf(j, "double", 2.92100f);
        json_kl(j, "long", 12345678912345);
        json_kb(j, "bool", true);
        json_key_obj(j, "object");
        json_end_obj(j);
        json_end_obj(j);
        json_key_arr(j, "others");
        json_v(j, "world");
        json_d(j, 2);
        json_f(j, 50.12300f);
        json_l(j, 987654321);
        json_b(j, false);
        json_obj(j);
        json_end_obj(j);
        json_arr(j);
        json_end_arr(j);
        json_end_arr(j);
        json_end_obj(j);
        json_end(j);

        ck_assert_str_eq(buf, "{\"tests\":{\"string\":\"hello\",\"integer\":-1,\"double\":2.92100,\"long\":12345678912345,\"bool\":true,\"object\":{}},\"others\":[\"world\",2,50.12300,987654321,false,{},[]]}");
    }
END_TEST

Suite *make_json_suite(void);

Suite *make_json_suite(void) {
    Suite *s = suite_create("json");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test  (tc_core, test_json);
    suite_add_tcase(s, tc_core);
    return s;
}
