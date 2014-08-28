#ifndef CFGNOD_H
#define CFGNOD_H 1

struct cfg;

const char *cfg_get_val(struct cfg *n, const char *key, const char *defval);
int cfg_get_int_val(struct cfg *n, const char *key, int defval);
float cfg_get_float_val(struct cfg *n, const char *key, float defval);

struct cfg *cfg_copy(struct cfg *from);
void cfg_destroy(struct cfg *cfg);
struct cfg *cfg_child(struct cfg *cfg);
struct cfg *cfg_next(struct cfg *cfg);
const char *cfg_name(struct cfg *cfg);

int cfg_set_value(struct cfg *, const char *key, const char *val);
int cfg_set_int_value(struct cfg *, const char *key, int val);
struct cfg *cfg_create_child(struct cfg *parent, const char *key);

#endif /* CFGNOD_H */
