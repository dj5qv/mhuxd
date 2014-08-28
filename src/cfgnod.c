#include <stdint.h>
#include <util/neo_hdf.h>
#include "cfgnod.h"
#include "logger.h"

const char *cfg_get_val(struct cfg *n, const char *key, const char *defval) {
	return hdf_get_value((void*)n, key, defval);
}
int cfg_get_int_val(struct cfg *n, const char *key, int defval) {
	return hdf_get_int_value((void*)n, key, defval);
}

float cfg_get_float_val(struct cfg *n, const char *key, float defval) {
	float f;
	const char *str = cfg_get_val(n, key, NULL);
	if(!str)
		return defval;

	char p[128];
	strncpy(p, str, sizeof(p)-1);
	p[sizeof(p)-1] = 0;

	char *ul = strchr(p, '_');
	if(ul)
		*ul = '.';

	sscanf(p, "%f", &f);
	return f;
}

int cfg_set_value(struct cfg *cfg, const char *key, const char *val) {
	NEOERR *err;
	err = hdf_set_value((void*)cfg, key, val);
	if(err != STATUS_OK) {
		err("(cfg) %s() could not set %s/%s", __func__, key, val);
		nerr_ignore(&err);
		return -1;
	}
	return 0;
}

int cfg_set_int_value(struct cfg *cfg, const char *key, int val) {
	NEOERR *err;
	//	dbg1("set %s/%d", key, val);
	err = hdf_set_int_value((void*)cfg, key, val);
	if(err != STATUS_OK) {
		err("(cfg) %s() could not set %s/%d", __func__, key, val);
		nerr_ignore(&err);
		return -1;
	}
	return 0;
}


struct cfg *cfg_copy(struct cfg *from) {
	NEOERR *err;
	HDF *dest;
	err = hdf_init(&dest);

	if(err == STATUS_OK)
		err = hdf_copy(dest, "", (void*)from);


	if(err != STATUS_OK) {
		hdf_destroy(&dest);
		err("(cfg) could not copy cfg tree!");
		nerr_ignore(&err);
		return NULL;
	}

	return (void *)dest;
}

struct cfg *cfg_create_child(struct cfg *parent, const char *key) {
	HDF *child;
	NEOERR *err;
	err = hdf_get_node((HDF *)parent, key, &child);
	if(err != STATUS_OK)
		child = NULL;
	return (struct cfg *)child;
}

void cfg_destroy(struct cfg *cfg) {
	if(!cfg)
		return;
	hdf_destroy((void*)&cfg);
}

struct cfg *cfg_child(struct cfg *cfg) {
	return (void*)hdf_obj_child((void*)cfg);
}

struct cfg *cfg_next(struct cfg *cfg) {
	return (void*)hdf_obj_next((void*)cfg);
}

const char *cfg_name(struct cfg *cfg) {
	return hdf_obj_name((void*)cfg);
}
