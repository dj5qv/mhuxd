/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2017  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include "clearsilver/util/neo_hdf.h"
#include "cfgnod.h"
#include "logger.h"

#define MOD_ID "cfg"

struct cfg *cfg_create() {
	NEOERR *err;
	HDF *hdf;
	err = hdf_init(&hdf);
	if(err != STATUS_OK) {
		err("%s() could not create HDF!", __func__);
		nerr_ignore(&err);
		return NULL;
	}
	return (struct cfg *)hdf;
}

void cfg_destroy(struct cfg *cfg) {
	if(cfg)
		hdf_destroy((HDF **)&cfg);
}

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
		err("%s() could not set %s/%s", __func__, key, val);
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
		err("%s() could not set %s/%d", __func__, key, val);
		nerr_ignore(&err);
		return -1;
	}
	return 0;
}

int cfg_set_int_val(struct cfg *cfg, const char *key, int val) {
	return cfg_set_int_value(cfg, key, val);
}

int cfg_set_float_val(struct cfg *cfg, const char *key, float val) {
	char buf[64];
	snprintf(buf, sizeof(buf), "%g", val);
	char *dot = strchr(buf, '.');
	if(dot)
		*dot = '_';
	return cfg_set_value(cfg, key, buf);
}


struct cfg *cfg_copy(struct cfg *from) {
	NEOERR *err;
	HDF *dest;
	err = hdf_init(&dest);

	if(err == STATUS_OK)
		err = hdf_copy(dest, "", (void*)from);


	if(err != STATUS_OK) {
		hdf_destroy(&dest);
		err("%s() could not copy cfg tree!", __func__);
		nerr_ignore(&err);
		return NULL;
	}

	return (void *)dest;
}

int cfg_merge_s(struct cfg *dest, const char *name, struct cfg *src) {
	NEOERR *err;
	err = hdf_copy((HDF *)dest, name, (HDF*)src);
	if(err != STATUS_OK) {
		nerr_ignore(&err);
		err("%s() could not merge!", __func__);
		return -1;
	}
	return 0;
}

int cfg_merge_i(struct cfg *dest, int name, struct cfg *src) {
	char buf[128];
	snprintf(buf, sizeof(buf), "%d", name);
	return cfg_merge_s(dest, buf, src);
}

int cfg_merge(struct cfg *dest, struct cfg *src) {
	return cfg_merge_s(dest, "", src);
}

struct cfg *cfg_create_child(struct cfg *parent, const char *key) {
	HDF *child;
	NEOERR *err;
	err = hdf_get_node((HDF *)parent, key, &child);
	if(err != STATUS_OK) {
		child = NULL;
		err("%s() could not create child %s!", __func__, key);
	}
	return (struct cfg *)child;
}

struct cfg *cfg_get_child(struct cfg *parent, const char *key) {
	return (struct cfg *)hdf_get_obj((HDF *)parent, key);
}

struct cfg *cfg_get_child_by_int(struct cfg *parent, int ikey) {
	char buf[128];
	snprintf(buf, sizeof(buf), "%d", ikey);
	return (struct cfg *)hdf_get_obj((HDF *)parent, buf);
}

struct cfg *cfg_first_child(struct cfg *cfg) {
	return (void*)hdf_obj_child((void*)cfg);
}

struct cfg *cfg_next_child(struct cfg *cfg) {
	return (void*)hdf_obj_next((void*)cfg);
}

const char *cfg_name(struct cfg *cfg) {
	return hdf_obj_name((void*)cfg);
}

int cfg_name_to_int(struct cfg *cfg, int def) {
	const char *name = cfg_name(cfg);
	if(!name)
		return def;
	return atoi(name);
}

int cfg_remove_child(struct cfg *cfg, const char *path, const char *name) {
	NEOERR *err;
	char buf[128];
	if(name && *name) {
		snprintf(buf, sizeof(buf), "%s.%s", path, name);
	} else {
		snprintf(buf, sizeof(buf), "%s", path);
	}

	err = hdf_remove_tree((HDF *)cfg, buf);
	if(err != STATUS_OK) {
		nerr_ignore(&err);
		return -1;
	}
	return 0;
}

int cfg_remove_child_i(struct cfg *cfg, const char *path, int i) {
	char buf[128];
	snprintf(buf, sizeof(buf), "%s.%d", path, i);
	return cfg_remove_child(cfg, buf, NULL);
}

