
/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef MHCONTROL_H
#define MHCONTROL_H

#include <stdint.h>

enum  {
	CMD_RESULT_INVALID,
	CMD_RESULT_OK,
	CMD_RESULT_TIMEOUT,
	CMD_RESULT_NOT_SUPPORTED,
};

enum {
	MHC_KEYER_STATE_UNKNOWN,
	MHC_KEYER_STATE_DISABLED,
	MHC_KEYER_STATE_OFFLINE,
	MHC_KEYER_STATE_DISC,
	MHC_KEYER_STATE_ONLINE,
};

typedef void (*cmd_completion_cb)(unsigned const char *reply, int len, int result, void *user_data);
typedef void (*mhc_state_changed_cb)(const char *serial, int state, void *user_data);

struct mh_control;
struct mh_router;
struct ev_loop;
struct cfg;
struct cfgmgr;

struct mh_control *mhc_create(struct ev_loop *loop, struct mh_router *router, uint16_t type);
void mhc_destroy(struct mh_control *ctl);
int mhc_is_connected(struct mh_control *ctl);
int mhc_is_online(struct mh_control *ctl);
const struct mh_info *mhc_get_mhinfo(struct mh_control *ctl);
int mhc_set_speed(struct mh_control *ctl, int channel, struct cfg *cfg, cmd_completion_cb cb, void *user_data);
uint16_t mhc_get_type(struct mh_control *);
const struct cfg *mhc_get_speed_args(struct mh_control *ctl, int channel);
int mhc_set_kopts(struct mh_control *ctl, struct cfg *);
int mhc_set_kopt(struct mh_control *ctl, const char *key, int val);
int mhc_load_kopts(struct mh_control *ctl, cmd_completion_cb cb, void *user_data);
int mhc_kopts_to_cfg(struct mh_control *ctl, struct cfg *cfg);
void mhc_add_state_changed_cb(struct mh_control *ctl, mhc_state_changed_cb cb, void *user_data);
void mhc_rem_state_changed_cb(struct mh_control *ctl, mhc_state_changed_cb cb);
uint8_t mhc_get_state(struct mh_control *ctl);
const char *mhc_state_str(int state);
int mhc_set_mode(struct mh_control *ctl, int mode, cmd_completion_cb cb, void *user_data);

#endif // MHCONTROL_H