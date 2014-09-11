/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef CFGMGR_H
#define CFGMGR_H

#include <stdint.h>

struct ev_loop;
struct cfgmgr;
struct mh_info;
struct conmgr;
struct cfg;

enum CFGMGR_APPLY_MODE {
	CFGMGR_APPLY_REPLACE,
	CFGMGR_APPLY_ADD,

	CFGMGR_SET,
	CFGMGR_REMOVE,
	CFGMGR_ADD,
	CFGMGR_ADD_ANT_REF,
	CFGMGR_REM_ANT_REF,

};

struct cfgmgr *cfgmgr_create(struct conmgr *, struct ev_loop *loop);
void cfgmgr_destroy(struct cfgmgr *cfgmgr);
int cfgmgr_init(struct cfgmgr *cfgmgr);
//int cfgmgr_update_hdf_all(struct cfgmgr *cfgmgr);
//int cfgmgr_update_hdf_dev(struct cfgmgr *cfgmgr, const char *serial);
//int cfgmgr_update_hdf_kopt(const char *serial, const char *key, int val);
int cfgmgr_merge_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg);
int cfgmgr_apply_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg, int apply_mode);
int cfgmgr_modify(struct cfgmgr *cfgmgr, struct cfg *cfg);
int cfgmgr_remove(struct cfgmgr *cfgmgr, struct cfg *cfg);
int cfgmgr_save_cfg(struct cfgmgr *cfgmgr);

void cfgmr_state_changed_cb(const char *serial, int state, void *user_data);

#endif // CFGMGR_H
