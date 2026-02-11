
/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef MHCONTROL_H
#define MHCONTROL_H

#include <stdint.h>

enum  {
	CMD_RESULT_OK,
	CMD_RESULT_ERROR,   // "some" error occured. Usually log file has meaningful error message.
	CMD_RESULT_TIMEOUT, // didn't get response in time from keyer.
	CMD_RESULT_NOT_SUPPORTED, // genuine response from keyer that command is not supported.
	CMD_RESULT_OFFLINE, // keyer is not online, can't send command.
};

// Keyer states, as returned by mhc_get_state() and in the keyer state change callback.
enum {
	MHC_KEYER_STATE_UNKNOWN,
	MHC_KEYER_STATE_DISABLED,
	MHC_KEYER_STATE_OFFLINE,
	MHC_KEYER_STATE_DISC,
	MHC_KEYER_STATE_ONLINE,
};

// Keyer modes.
enum {
    MOD_CW = 0,
    MOD_VOICE = 1,
    MOD_FSK = 2,
    MOD_DIGITAL = 3
};


typedef void (*mhc_cmd_completion_cb_fn)(unsigned const char *reply, int len, int result, void *user_data);
typedef void (*mhc_keyer_state_cb_fn)(const char *serial, int mhc_keyer_state, void *user_data);
typedef void (*mhc_state_cb_fn)(const char *serial, const uint8_t *state, uint8_t state_len, void *user_data);
typedef void (*mhc_mode_cb_fn)(const char *serial, uint8_t mode_cur, uint8_t mode_r1, uint8_t mode_r2, void *user_data);

struct mh_control;
struct mh_router;
struct ev_loop;
struct cfg;
struct cfgmgr;
struct mh_info;
struct mhc_keyer_state_callback;
struct mhc_state_callback;
struct mhc_mode_callback;

struct mh_control *mhc_create(struct ev_loop *loop, struct mh_router *router, struct mh_info *mhi);
void mhc_destroy(struct mh_control *ctl);
int mhc_get_keyer_mode(struct mh_control *ctl);
int mhc_is_connected(struct mh_control *ctl);
int mhc_is_online(struct mh_control *ctl);
const char *mhc_get_serial(struct mh_control *ctl);
const struct mh_info *mhc_get_mhinfo(struct mh_control *ctl);
uint16_t mhc_get_type(struct mh_control *);
const struct cfg *mhc_get_speed_cfg(struct mh_control *ctl, int channel);

struct mhc_speed_cfg {
	double baud;
	double stopbits;
	int databits;
	int rtscts;
	int rigtype;
	int icomaddress;
	int icomsimulateautoinfo;
	int digitalovervoicerule;
	int usedecoderifconnected;
	int dontinterfereusbcontrol;
};

// async functions, will always call the callback even if the async operation could be started.
void mhc_set_speed(struct mh_control *ctl, int channel, struct cfg *cfg, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_set_speed_params(struct mh_control *ctl, int channel, const struct mhc_speed_cfg *cfg, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_load_kopts(struct mh_control *ctl, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_set_mode(struct mh_control *ctl, int mode, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_record_message(struct mh_control *ctl, uint8_t idx, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_stop_recording(struct mh_control *ctl, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_play_message(struct mh_control *ctl, uint8_t idx, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_abort_message(struct mh_control *ctl, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_store_cw_message(struct mh_control *ctl, uint8_t idx, const char *text, uint8_t next_idx, uint8_t delay, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_store_fsk_message(struct mh_control *ctl, uint8_t idx, const char *text, uint8_t next_idx, uint8_t delay, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_mk2r_set_hfocus(struct mh_control *ctl, uint8_t hfocus[8], mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_mk2r_set_acc_outputs(struct mh_control *ctl, uint8_t acc_outputs[4], mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_mk2r_set_scenario(struct mh_control *ctl, uint8_t idx, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_sm_turn_to_azimuth(struct mh_control *ctl, uint16_t bearing, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_sm_get_antsw_block(struct mh_control *ctl, uint16_t offset, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_sm_set_antsw_validity(struct mh_control *ctl, uint8_t param, mhc_cmd_completion_cb_fn cb, void *user_data);
void mhc_sm_store_antsw_block(struct mh_control *ctl, uint16_t offset, const char *data, mhc_cmd_completion_cb_fn cb, void *user_data);

// set keyer option in mhuxd memory, not sending it to keyer. For that use mhc_load_kopts().
int mhc_set_kopt(struct mh_control *ctl, const char *key, int val);

// getters
uint8_t mhc_get_state(struct mh_control *ctl);
int mhc_get_speed_params(struct mh_control *ctl, int channel, struct mhc_speed_cfg *out);
int mhc_kopts_to_cfg(struct mh_control *ctl, struct cfg *cfg);
int mhc_kopts_foreach(struct mh_control *ctl, int (*cb)(const char *key, int val, void *user_data), void *user_data);
const char *mhc_get_cw_message(struct mh_control *ctl, uint8_t idx, uint8_t *next_idx_out, uint8_t *delay_out);
const char *mhc_get_fsk_message(struct mh_control *ctl, uint8_t idx, uint8_t *next_idx_out, uint8_t *delay_out);
void mhc_mk2r_get_hfocus(struct mh_control *ctl, uint8_t dest[8]);
void mhc_mk2r_get_acc_outputs(struct mh_control *ctl, uint8_t dest[4]);
struct sm *mhc_get_sm(struct mh_control *ctl);
struct mh_router *mhc_get_router(struct mh_control *ctl);

// callback installation / removal
struct mhc_keyer_state_callback *mhc_add_keyer_state_changed_cb(struct mh_control *ctl, mhc_keyer_state_cb_fn func, void *user_data);
void mhc_rem_keyer_state_changed_cb(struct mh_control *ctl, struct mhc_keyer_state_callback *);
struct mhc_state_callback *mhc_add_mok_state_changed_cb(struct mh_control *ctl, mhc_state_cb_fn func, void *user_data);
void mhc_rem_mok_state_changed_cb(struct mh_control *ctl, struct mhc_state_callback *);
struct mhc_state_callback *mhc_add_acc_state_changed_cb(struct mh_control *ctl, mhc_state_cb_fn func, void *user_data);
void mhc_rem_acc_state_changed_cb(struct mh_control *ctl, struct mhc_state_callback *);
struct mhc_mode_callback *mhc_add_mode_changed_cb(struct mh_control *ctl, mhc_mode_cb_fn func, void *user_data);
void mhc_rem_mode_changed_cb(struct mh_control *ctl, struct mhc_mode_callback *);

// state to string conversions
const char *mhc_state_str(int state);
const char *mhc_cmd_err_string(int result);





#endif // MHCONTROL_H
