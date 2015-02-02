#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

typedef struct json json_t;
struct json {
    char *buf_start;
    char *buf_end;
    char *b;
    bool overflowed;
    bool in_list;
};

void json_init (json_t *j, char *buf, size_t buflen);
void json_kv(json_t *j, char *key, const char *value);
void json_kd(json_t *j, char *key, int value);
void json_kf(json_t *j, char *key, double value);
void json_kl(json_t *j, char *key, int64_t value);
void json_kb(json_t *j, char *key, bool value);
void json_key_obj(json_t *j, char *key);
void json_key_arr(json_t *j, char *key);
void json_obj(json_t *j);
void json_arr(json_t *j);
void json_end_obj(json_t *j);
void json_end_arr(json_t *j);
void json_dump(json_t *j);
size_t json_length(json_t *j);
