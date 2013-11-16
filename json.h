#include "router.h"

uint32_t render_plan_json(struct plan *plan, tdata_t *tdata, char *buf, uint32_t buflen);

void json_begin(char *buf, size_t buflen);

void json_dump();

size_t json_length();

void json_kv(char *key, char *value);

void json_kd(char *key, int value);

void json_kf(char *key, double value);

void json_kl(char *key, int64_t value);

void json_kb(char *key, bool value);

void json_obj();

void json_arr();

void json_key_obj(char *key);

void json_key_arr(char *key);

void json_end_obj();

void json_end_arr();

void json_string (char *str);
