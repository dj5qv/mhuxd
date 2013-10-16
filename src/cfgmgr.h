#ifndef CFGMGR_H
#define CFGMGR_H

#include <stdint.h>

struct ev_loop;
struct cfgmgr;
struct mh_info;
struct conmgr;

struct cfgmgr *cfgmgr_create(struct conmgr *, struct ev_loop *loop);
void cfgmgr_destroy(struct cfgmgr *cfgmgr);
int cfgmgr_init(struct cfgmgr *cfgmgr);
int cfgmgr_update_hdf_all(struct cfgmgr *cfgmgr);
int cfgmgr_update_hdf_dev(struct cfgmgr *cfgmgr, const char *serial);
//int cfgmgr_update_hdf_kopt(const char *serial, const char *key, int val);
struct cfg *cfgmgr_get_live_cfg(struct cfgmgr *cfgmgr);
int cfgmgr_apply_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg);
int cfgmgr_unset_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg);
int cfgmgr_save_cfg(struct cfgmgr *cfgmgr);

void cfgmr_state_changed_cb(const char *serial, int state, void *user_data);

#endif // CFGMGR_H
