/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <ev.h>
#include "mhcontrol.h"
#include "mhflags.h"
#include "util.h"
#include "pglist.h"
#include "buffer.h"
#include "logger.h"
#include "channel.h"
#include "mhrouter.h"
#include "mhinfo.h"
#include "kcfg.h"
#include "cfgnod.h"
#include "mhmk2r.h"

#define MAX_CMD_LEN 128
#define MAX_CMD_QUEUE_SIZE 16
#define MSB_BIT (1<<7)

#define IVAL_HEARTBEAT 2.0
#define IVAL_INFO 2
#define CMD_TIMEOUT 1.0

// microHam commands
enum {
	MHCMD_NOP             = 0x00,
	MHCMD_SET_CHANNEL_R1  = 0x01,
	MHCMD_SET_CHANNEL_AUX_R2  = 0x02,
	MHCMD_SET_CHANNEL_FSK_R1  = 0x03,
	MHCMD_SET_CHANNEL_FSK_R2  = 0x04,
	MHCMD_GET_VERSION         = 0x05,
	MHCMD_START_BOOTLOADER    = 0x06,
	MHCMD_JUST_RESTARTED      = 0x07,
	MHCMD_STORE_SETTINGS      = 0x08,
	MHCMD_SET_SETTINGS        = 0x09,
	MHCMD_SET_KEYER_MODE      = 0x0a,
	MHCMD_STORE_WINKEY_INIT   = 0x0b,
	MHCMD_RECORD_FSK_CW_MSG   = 0x0c,
	MHCMD_PLAY_FSK_CW_MSG     = 0x0d,
	MHCMD_ABORT_FSK_CW_MSG    = 0x0e,
	MHCMD_WINKEY_NO_RESPONSE  = 0x0f,
	MHCMD_ON_CONNECT          = 0x1a,
	MHCMD_STORE_CHANNEL_R1    = 0x1b,
	MHCMD_STORE_CHANNEL_R2    = 0x1c,
	MHCMD_PRESET_NAME         = 0x1d, /* MK2 only */
	MHCMD_CAT_R1_FR_MODE_INFO = 0x1e,
	MHCMD_CAT_R1_FREQUENCY_INFO = 0x1f,
	MHCMD_STORE_FSK_MSG_1     = 0x20, /* MK2, MK2R only */
	MHCMD_STORE_FSK_MSG_2     = 0x21,
	MHCMD_STORE_FSK_MSG_3     = 0x22,
	MHCMD_STORE_FSK_MSG_4     = 0x23,
	MHCMD_STORE_FSK_MSG_5     = 0x24,
	MHCMD_STORE_FSK_MSG_6     = 0x25,
	MHCMD_STORE_FSK_MSG_7     = 0x26,
	MHCMD_STORE_FSK_MSG_8     = 0x27,
	MHCMD_STORE_FSK_MSG_9     = 0x28,
	MHCMD_DVK_REC             = 0x29, /* MK2 only */
	MHCMD_DVK_PLAY            = 0x2a, /* MK2 only */
	MHCMD_DISPLAY_HOST_STRING_EVENT = 0x2b, /* SM, MK2 only */
	MHCMD_DISPLAY_HOST_STRING = 0x2c, /* SM, MK2 only */
	MHCMD_CANCEL_HOST_STRING  = 0x2d, /* SM, MK2 only */
	MHCMD_STORE_DISPLAY_STRING = 0x2e, /* SM, MK2 only */
	MHCMD_GET_DISPLAY_STRING  = 0x2f, /* SM, MK2 only */
	MHCMD_HOST_FOCUS_CONTROL  = 0x31, /* MK2R, U2R only */
	MHCMD_STORE_SCENARIO      = 0x32, /* MK2R, U2R only */
	MHCMD_GET_SCENARIO        = 0x33, /* MK2R, U2R only */
	MHCMD_APPLY_SCENARIO      = 0x34, /* MK2R, U2R only */
	MHCMD_HOST_ACC_OUTPUTS_CONTROL = 0x35, /* MK2R, U2R only */
	MHCMD_CAT_R2_FR_MODE_INFO = 0x36, /* MK2R only */
	MHCMD_CAT_R2_FREQUENCY_INFO = 0x37, /* MK2R only */

	/* 0x41 - 0x4D, 0x76 StationMaster only, not supported yet. */

	MHCMD_U2R_STATE           = 0x75,  /* U2R only */
	MHCMD_USB_RX_OVERFLOW     = 0x77,
	MHCMD_MPK_STATE           = 0x78, /* MK, DK2 only */
	MHCMD_ACC_STATE           = 0x79, /* U2R, MK2R only */
	MHCMD_DVK_CONTROL         = 0x7a, /* MK2R only */
	MHCMD_MOK_STATE           = 0x7b, /* MK2R only */

	MHCMD_KEYER_MODE          = 0x7c,
	MHCMD_AUTO_NUMBER         = 0x7d,
	MHCMD_ARE_YOU_THERE       = 0x7e,
	MHCMD_NOT_SUPPORTED       = 0x7f
};

enum {
	CTL_STATE_DEVICE_DISABLED,
	CTL_STATE_DEVICE_OFF,
	CTL_STATE_DEVICE_DISC,
	CTL_STATE_INIT,
	CTL_STATE_GET_TYPE,
	CTL_STATE_SET_CHANNELS,
	CTL_STATE_LOAD_CFG,
	CTL_STATE_ON_CONNECT,
	CTL_STATE_OK,
};

enum {
	CMD_STATE_QUEUED,
	CMD_STATE_SENT,
};

struct state_changed_cb {
	struct PGNode node;
	mhc_state_changed_cb cb;
	void *user_data;
};

struct mh_control {
	const char *serial;
	struct mh_router *router;
	struct ev_loop *loop;
	struct kcfg *kcfg;
	struct PGList cmd_list;
	struct PGList free_list;
	struct PGList state_changed_cb_list;
	//	mhc_state_changed_cb state_changed_cb;
	//	void *state_chaned_cb_user_data;

	ev_timer heartbeat_timer;
	ev_timer cmd_timeout_timer;
	struct mh_info mhi;
	struct cfg *speed_args[MH_NUM_CHANNELS];
	uint8_t speed_idx;
	uint8_t state;
	uint8_t keyer_state;
	uint8_t in_flag_r1, in_flag_r2;
	uint8_t out_flag;
	uint8_t set_mode;
	uint8_t mpk_mok_state[8];
	uint8_t acc_state[8];
	uint8_t hfocus[8];
};

struct command {
	struct PGNode node;
	uint16_t len;
	uint8_t state;
	mhc_cmd_completion_cb cmd_completion_cb;
	void *user_data;
	uint8_t cmd[MAX_CMD_LEN];
};

static int submit_cmd(struct mh_control *ctl, struct buffer *b, mhc_cmd_completion_cb cb, void *user_data);
static int submit_cmd_simple(struct mh_control *ctl, int cmd, mhc_cmd_completion_cb cb, void *user_data);
static int submit_speed_cmd(struct mh_control *ctl, int channel, mhc_cmd_completion_cb cb, void *user_data);
static void initializer_cb(unsigned const char *reply, int len, int result, void *user_data);
static int push_cmds(struct mh_control *ctl);

static const char *state_strings[] = {
	[MHC_KEYER_STATE_UNKNOWN] = "UNKNOWN",
	[MHC_KEYER_STATE_DISABLED] = "DISABLED",
	[MHC_KEYER_STATE_OFFLINE] = "OFFLINE",
	[MHC_KEYER_STATE_DISC] = "DISCONNECTED",
	[MHC_KEYER_STATE_ONLINE] = "ONLINE",
};

uint8_t mhc_get_state(struct mh_control *ctl) {
	return ctl->keyer_state;
}

const char *mhc_state_str(int state) {
	if(state >= (int)ARRAY_SIZE(state_strings))
		state = MHC_KEYER_STATE_UNKNOWN;
	return state_strings[state];
}

static uint8_t state_to_ext_state(uint8_t state) {
	uint8_t ext_state;

	switch(state) {
	case CTL_STATE_DEVICE_DISABLED:
		ext_state = MHC_KEYER_STATE_DISABLED;
		break;
	case CTL_STATE_DEVICE_OFF:
	case CTL_STATE_GET_TYPE:
	case CTL_STATE_INIT:
	case CTL_STATE_SET_CHANNELS:
	case CTL_STATE_LOAD_CFG:
	case CTL_STATE_ON_CONNECT:
		ext_state = MHC_KEYER_STATE_OFFLINE;
		break;
	case CTL_STATE_DEVICE_DISC:
		ext_state = MHC_KEYER_STATE_DISC;
		break;
	case CTL_STATE_OK:
		ext_state = MHC_KEYER_STATE_ONLINE;
		break;
	default:
		err("(mhc) invalid CTL state %d!", state);
		ext_state = MHC_KEYER_STATE_UNKNOWN;
	}

	return ext_state;
}

static void set_state(struct mh_control *ctl, uint8_t state) {
	struct state_changed_cb *sccb;
	uint8_t new_keyer_state;

	ctl->state = state;

	if(state == CTL_STATE_DEVICE_DISC)
		ev_timer_stop(ctl->loop, &ctl->heartbeat_timer);
	else
		ev_timer_start(ctl->loop, &ctl->heartbeat_timer);

	new_keyer_state = state_to_ext_state(state);

	if(new_keyer_state != ctl->keyer_state) {
		ctl->keyer_state = new_keyer_state;
		PG_SCANLIST(&ctl->state_changed_cb_list, sccb)
			if(sccb->cb)
				sccb->cb(ctl->serial, ctl->keyer_state, sccb->user_data);
	}
}

static void generic_cb(unsigned const char *reply, int len, int result, void *user_data) {
	(void)len;(void)user_data;(void)reply,(void)result;
}

static void heartbeat_completed_cb(unsigned const char *reply, int len, int result, void *user_data) {
	(void)reply; (void)len; (void)user_data;
	struct mh_control *ctl = user_data;

	if(result == CMD_RESULT_OK) {
		dbg0("(mhc) heartbeat pong");

		if(ctl->state == CTL_STATE_DEVICE_OFF) {
			initializer_cb(NULL, 0, CMD_RESULT_INVALID, ctl);
		}
		return;
	}
	if(result == CMD_RESULT_TIMEOUT) {
		if(ctl->state != CTL_STATE_DEVICE_OFF) {
			set_state(ctl,CTL_STATE_DEVICE_OFF);
			warn("(mhc) %s heartbeat timed out!", ctl->serial);
			info("(mhc) %s OFFLINE", ctl->serial);
		} else {
			dbg0("(mhc) %s heartbeat timed out!", ctl->serial);
		}
		return;
	}

	err("(mhc) heartbeat invalid result!");
}

static void heartbeat_cb (struct ev_loop *loop,  struct ev_timer *w, int revents) {
	(void)loop;(void)revents;
	struct mh_control *ctl = w->data;
	dbg0("(mhc) heartbeat ping");
	submit_cmd_simple(ctl, MHCMD_ARE_YOU_THERE, heartbeat_completed_cb, ctl);
}

static void cmd_timeout_cb (struct ev_loop *loop,  struct ev_timer *w, int revents) {
	(void)loop; (void)revents;
	struct mh_control *ctl = w->data;
	struct command *cmd = (void*)PG_FIRSTENTRY(&ctl->cmd_list);

	if(cmd && cmd->state == CMD_STATE_SENT) {
		dbg1_h("(mhc) command timed out: ", cmd->cmd, cmd->len);
		PG_Remove(&cmd->node);
		if(cmd->cmd_completion_cb)
			cmd->cmd_completion_cb(NULL, 0, CMD_RESULT_TIMEOUT, cmd->user_data);

		PG_AddTail(&ctl->free_list, &cmd->node);
		push_cmds(ctl);
		return;
	}
	err("(mhc) timeout received with no command pending!");
}

static const char *keyer_modes[] = {
        "CW", "VOICE", "FSK", "DIGITAL",
};

/*
 *  Process MPK State, MOK State, ACC State and U2R State
 */
static void process_keyer_states(struct mh_control *ctl, unsigned const char *data, int len) {

	if(*data == MHCMD_MPK_STATE && (ctl->mhi.type == MHT_MK2 || ctl->mhi.type == MHT_DK2)) {
		if(len != 6) {
			err("(mhc) invalid cmd length for MPK/DK2 state! cmd: %d len: %d", *data, len);
			return;
		}
		memcpy(ctl->mpk_mok_state, data + 1, 8);
		mk2_debug_print_mpk_values(ctl->mpk_mok_state);
		return;
	}

	if(*data == MHCMD_MOK_STATE && (ctl->mhi.type == MHT_MK2R || ctl->mhi.type == MHT_MK2Rp)) {
		if(len != 10) {
			err("(mhc) invalid cmd length for MOK state! cmd: %d len: %d", *data, len);
			return;
		}

		memcpy(ctl->mpk_mok_state, data + 1, 8);
		mk2r_debug_print_mok_values(ctl->mpk_mok_state);
		return;
	}

	if(*data == MHCMD_ACC_STATE && (ctl->mhi.type == MHT_MK2R || ctl->mhi.type == MHT_MK2Rp)) {
		if(len != 10) {
			err("(mhc) invalid cmd length for ACC state! cmd: %d len: %d", *data, len);
			return;
		}

		memcpy(ctl->acc_state, data + 1, 8);
		return;
	}

	err("(mhc) invalid state cmd %d for keyer %s", *data, ctl->serial);
}

static void consumer_cb(struct mh_router *router, unsigned const char *data ,int len, int channel, void *user_data) {
	(void)router; (void)channel;
	struct mh_control *ctl = user_data;
	struct command *cmd = (void*)PG_FIRSTENTRY(&ctl->cmd_list);

	if(cmd && cmd->state == CMD_STATE_SENT && data[0] == cmd->cmd[0]) {
		ev_timer_stop(ctl->loop, &ctl->cmd_timeout_timer);
		PG_Remove(&cmd->node);
		dbg1_h("(mhc) cmd fm k: ", data, len);
		if(cmd->cmd_completion_cb)
			cmd->cmd_completion_cb(data, len, CMD_RESULT_OK, cmd->user_data);

		PG_AddTail(&ctl->free_list, &cmd->node);
		goto out;
	}

	if(cmd && cmd->state == CMD_STATE_SENT && data[0] == MHCMD_NOT_SUPPORTED) {
		ev_timer_stop(ctl->loop, &ctl->cmd_timeout_timer);
		PG_Remove(&cmd->node);
		if(cmd->cmd_completion_cb)
			cmd->cmd_completion_cb(data, len, CMD_RESULT_NOT_SUPPORTED, cmd->user_data);

		PG_AddTail(&ctl->free_list, &cmd->node);
		goto out;
	}

	int f;

	switch(*data) {
	case MHCMD_MPK_STATE:
	case MHCMD_MOK_STATE:
	case MHCMD_ACC_STATE:
		process_keyer_states(ctl, data, len);
		break;

	case MHCMD_KEYER_MODE:
		f = data[1];
		dbg0("(mhc) %s mode  cur: %s, r1: %s, r2: %s", ctl->serial,
		     keyer_modes[f & 3], keyer_modes[(f>>2) & 3],
		     keyer_modes[(f>>4) & 3]);
		if(ctl->kcfg)
			kcfg_update_keyer_mode(ctl->kcfg, f & 3, (f>>2) & 3, (f>>4) & 3);
		break;
		
	default:
		break;
	}

out:
	if(ctl->state == CTL_STATE_DEVICE_OFF) {
		initializer_cb(NULL, 0, CMD_RESULT_INVALID, ctl);
	}

	push_cmds(ctl);
}

static void initializer_cb(unsigned const char *reply, int len, int result, void *user_data) {
	struct mh_control *ctl = user_data;

	// previous step failed?
	if(result != CMD_RESULT_OK && result != CMD_RESULT_INVALID) {
		if(result == CMD_RESULT_TIMEOUT) {
			dbg0("(mhc) %s initializer timed out", ctl->serial);
		}
		if(result == CMD_RESULT_NOT_SUPPORTED) {
			dbg0("(mhc) %s cmd not supported!", ctl->serial);

		}
		set_state(ctl, CTL_STATE_DEVICE_OFF);
		info("(mhc) %s OFFLINE", ctl->serial);
		return;
	}


	switch(ctl->state) {
	case CTL_STATE_DEVICE_OFF:
		// kick off the state machine
		info("(mhc) %s INITIALIZING", ctl->serial);
		dbg0("(mhc) %s initializer ping", ctl->serial);
		set_state(ctl, CTL_STATE_INIT);
		submit_cmd_simple(ctl, MHCMD_ARE_YOU_THERE, initializer_cb, ctl);
		break;
	case CTL_STATE_INIT:
		dbg0("(mhc) %s initializer ping ok", ctl->serial);
		set_state(ctl, CTL_STATE_GET_TYPE);
		submit_cmd_simple(ctl, MHCMD_GET_VERSION, initializer_cb, ctl);
		dbg0("(mhc) %s get version", ctl->serial);
		break;
	case CTL_STATE_GET_TYPE:
		mhi_parse_version(&ctl->mhi, reply, len);
		info("(mhc) %s detected microHam %s firmware %d.%d", ctl->serial,
		     ctl->mhi.type_str, ctl->mhi.ver_fw_major, ctl->mhi.ver_fw_minor);

		// fall through
	case CTL_STATE_SET_CHANNELS:
		// speeds
		if(ctl->state == CTL_STATE_SET_CHANNELS)
			dbg0("(mhc) %s set channel %d ok", ctl->serial, ctl->speed_idx - 1);

		while(ctl->speed_idx < MH_NUM_CHANNELS) {
			if(ctl->speed_args[ctl->speed_idx]) {
				set_state(ctl, CTL_STATE_SET_CHANNELS);
				dbg0("(mhc) %s set channel %d", ctl->serial, ctl->speed_idx);
				submit_speed_cmd(ctl, ctl->speed_idx, initializer_cb, ctl);
				ctl->speed_idx++;
				break;
			}
			ctl->speed_idx++;
		}

		if(ctl->speed_idx < MH_NUM_CHANNELS)
			break;

		ctl->speed_idx = 0;

		// kcfg
		if(ctl->kcfg == NULL)
			ctl->kcfg = kcfg_create(&ctl->mhi);

		if(ctl->kcfg == NULL) {
			err("(mhc) could not create keyer config!");
			set_state(ctl, CTL_STATE_DEVICE_DISABLED);
			break;
		}
		struct buffer *kb = kcfg_get_buffer(ctl->kcfg);
		struct buffer buf;
		buf_reset(&buf);
		buf_append_c(&buf, MHCMD_SET_SETTINGS);
		buf_append(&buf, kb->data, kb->size);
		buf_append_c(&buf, MHCMD_SET_SETTINGS | MSB_BIT);
		submit_cmd(ctl, &buf, initializer_cb, ctl);
		set_state(ctl, CTL_STATE_LOAD_CFG);
		dbg0("(mhc) %s upload config", ctl->serial);
		break;
	case CTL_STATE_LOAD_CFG:
		dbg0("(mhc) %s requesting status information", ctl->serial);
		submit_cmd_simple(ctl, MHCMD_ON_CONNECT, initializer_cb, ctl);
		set_state(ctl, CTL_STATE_ON_CONNECT);
		break;
	case CTL_STATE_ON_CONNECT:
		info("(mhc) %s ONLINE", ctl->serial);
		set_state(ctl, CTL_STATE_OK);
	}
}

static void flags_cb(struct mh_router *router, const uint8_t *data ,int len, int channel, void *user_data) {
	(void)router; (void)(channel);
	struct mh_control *ctl = user_data;
	int i;

	for(i=0; i<len; i++) {
		uint8_t *old_flag;
		int radio;
		if(data[i] & MHD2CFL_R2) {
			old_flag = &ctl->in_flag_r2;
			radio = 2;
		} else {
			old_flag = &ctl->in_flag_r1;
			radio = 1;
		}

		if((*old_flag & MHD2CFL_CTS) != (data[i] & MHD2CFL_CTS)) {
			dbg0("(mhc) >> %s fl cts r%d %d", ctl->serial, radio, (data[i] & MHD2CFL_CTS) ? 1 : 0);
			if(!(data[i] & MHD2CFL_CTS)) {
				warn("(mhc) %s CTS went from 1 to 0, CTS handling not implemented!", ctl->serial);
			}
		}
		if((*old_flag & MHD2CFL_LOCKOUT) != (data[i] & MHD2CFL_LOCKOUT))
			dbg0("(mhc) %s fl lockout r%d %d", ctl->serial, radio, (data[i] & MHD2CFL_LOCKOUT) ? 1 : 0);
		if((*old_flag & MHD2CFL_ANY_PTT) != (data[i] & MHD2CFL_ANY_PTT))
			dbg0("(mhc) %s fl ptt-any r%d %d", ctl->serial, radio, (data[i] & MHD2CFL_ANY_PTT) ? 1 : 0);
		if((*old_flag & MHD2CFL_SQUELCH) != (data[i] & MHD2CFL_SQUELCH))
			dbg0("(mhc) %s fl squelch r%d %d", ctl->serial, radio, (data[i] & MHD2CFL_SQUELCH) ? 1 : 0);
		if((*old_flag & MHD2CFL_FSK_BUSY) != (data[i] & MHD2CFL_FSK_BUSY))
			dbg0("(mhc) %s fl fsk-busy r%d %d", ctl->serial, radio, (data[i] & MHD2CFL_FSK_BUSY) ? 1 : 0);
		if((*old_flag & MHD2CFL_NON_VOX_PTT) != (data[i] & MHD2CFL_NON_VOX_PTT))
			dbg0("(mhc) %s fl non-vox-ptt r%d %d", ctl->serial, radio, (data[i] & MHD2CFL_NON_VOX_PTT) ? 1 : 0);
		if((*old_flag & MHD2CFL_FOOTSWITCH) != (data[i] & MHD2CFL_FOOTSWITCH))
			dbg0("(mhc) %s non-vox-ptt r%d %d", ctl->serial, radio, (data[i] & MHD2CFL_FOOTSWITCH) ? 1 : 0);

		*old_flag = data[i];
	}
}

static void router_status_cb(struct mh_router *router, int status, void *user_data) {
	(void)router;

	struct mh_control *ctl = user_data;
	if(status == MHROUTER_CONNECTED) {
		switch(ctl->state) {
		case CTL_STATE_DEVICE_DISC:
		case CTL_STATE_DEVICE_OFF:
			// kick off state machine
			dbg0("(mhc) %s initializing", ctl->serial);
			set_state(ctl, CTL_STATE_DEVICE_OFF);
			initializer_cb(NULL, 0, CMD_RESULT_INVALID, ctl);
			break;

		}
	}
	if(status == MHROUTER_DISCONNECTED) {
		set_state(ctl, CTL_STATE_DEVICE_DISC);
		info("(mhc) %s DISCONNECTED", ctl->serial);
	}
}


struct mh_control *mhc_create(struct ev_loop *loop, struct mh_router *router, struct mh_info *mhi) {
	struct mh_control *ctl;

	dbg1("(mhc) %s()", __func__);

	ctl = w_calloc(1, sizeof(*ctl));
	ctl->loop = loop;
	ctl->router = router;
	ctl->serial = mhr_get_serial(router);
	ctl->set_mode = -1;

	PG_NewList(&ctl->cmd_list);
	PG_NewList(&ctl->free_list);
	PG_NewList(&ctl->state_changed_cb_list);

	set_state(ctl, CTL_STATE_DEVICE_DISC);

	memcpy(&ctl->mhi, mhi, sizeof(ctl->mhi));

	ctl->kcfg = kcfg_create(&ctl->mhi);

	ev_timer_init(&ctl->heartbeat_timer, heartbeat_cb, 0., IVAL_HEARTBEAT);
	ctl->heartbeat_timer.data = ctl;

	ev_timer_init(&ctl->cmd_timeout_timer, cmd_timeout_cb, CMD_TIMEOUT, 0.);
	ctl->cmd_timeout_timer.data = ctl;

	mhr_add_status_cb(router, router_status_cb, ctl);

	mhr_add_consumer_cb(router, consumer_cb, MH_CHANNEL_CONTROL, ctl);
	mhr_add_consumer_cb(router, flags_cb, MH_CHANNEL_FLAGS, ctl);

	return ctl;
}

void mhc_destroy(struct mh_control *ctl) {
	struct command *cmd;
	struct state_changed_cb *sccb;
	int i;

	dbg1("(mhc) %s()", __func__);

	ev_timer_stop(ctl->loop, &ctl->heartbeat_timer);
	ev_timer_stop(ctl->loop, &ctl->cmd_timeout_timer);

	mhr_rem_status_cb(ctl->router, router_status_cb);

	while((sccb = (void*)PG_FIRSTENTRY(&ctl->state_changed_cb_list))) {
		PG_Remove(&sccb->node);
		free(sccb);
	}

	while((cmd = (void*)PG_FIRSTENTRY(&ctl->cmd_list))) {
		PG_Remove(&cmd->node);
		free(cmd);
	}
	while((cmd = (void*)PG_FIRSTENTRY(&ctl->free_list))) {
		PG_Remove(&cmd->node);
		free(cmd);
	}

	if(ctl->kcfg)
		kcfg_destroy(ctl->kcfg);

	for(i = 0; i < MH_NUM_CHANNELS; i++) {
		if(ctl->speed_args[i])
			cfg_destroy(ctl->speed_args[i]);
	}

	free(ctl);
}

int mhc_is_connected(struct mh_control *ctl) {
	return ctl->state != CTL_STATE_DEVICE_DISC;
}

int mhc_is_online(struct mh_control *ctl) {
	return ctl->state == CTL_STATE_OK;
}

const struct mh_info *mhc_get_mhinfo(struct mh_control *ctl)  {
	return &ctl->mhi;
}

static int submit_speed_cmd(struct mh_control *ctl, int channel, mhc_cmd_completion_cb cb, void *user_data) {
	float fbaud, stopbits;
	int ibaud;
	uint8_t c, cmd, rtscts, databits, has_ext;

	dbg1("(mhc) %s %s()", ctl->serial, __func__);

	if(channel < 0 || channel >= MH_NUM_CHANNELS) {
		err("(mhc) %s() invalid channel number %d!", __func__, channel);
		return -1;
	}

	struct cfg *cfg = ctl->speed_args[channel];
	if(!cfg) {
		err("(mhc) %s() no speed args defined!", __func__);
		return -1;
	}

	struct buffer buf;
	buf_reset(&buf);

	fbaud = cfg_get_float_val(cfg, "baud", -1);

	has_ext = 0;

	switch(channel) {
		case MH_CHANNEL_R1:
			ibaud = 11059200 / fbaud;
			cmd = MHCMD_SET_CHANNEL_R1;
			has_ext = ctl->mhi.flags & MHF_HAS_R1_RADIO_SUPPORT;
			break;
		case MH_CHANNEL_R2:
			ibaud = 11059200 / fbaud;
			cmd = MHCMD_SET_CHANNEL_AUX_R2;
			has_ext = ctl->mhi.flags & MHF_HAS_R2_RADIO_SUPPORT;
			break;
		case MH_CHANNEL_R1_FSK:
			ibaud = 2700 / fbaud;
			cmd = MHCMD_SET_CHANNEL_FSK_R1;
			break;
		case MH_CHANNEL_R2_FSK:
			ibaud = 2700 / fbaud;
			cmd = MHCMD_SET_CHANNEL_FSK_R2;
			break;
		default:
			err("(mhc) can't set speed, invalid channel specified (%d)!", channel);
			return -1;
	}

	stopbits = cfg_get_float_val(cfg, "stopbits", 1);
	if(stopbits == 1.5)
		stopbits = 3;
	rtscts = cfg_get_int_val(cfg, "rtscts", 0);
	databits = cfg_get_int_val(cfg, "databits", 8);

	buf_append_c(&buf, cmd);
	buf_append_c(&buf, ibaud & 0xff);
	buf_append_c(&buf, ibaud >> 8);
	c = (((int)stopbits) - 1) << 2;
	c |= rtscts;
	c |= (databits - 5) << 5;
	buf_append_c(&buf, c);

	if(has_ext) {
		c = cfg_get_int_val(cfg, "rigtype", 0);
		buf_append_c(&buf, c);
		c = cfg_get_int_val(cfg, "icomaddress", 0);
		buf_append_c(&buf, c);
		c = cfg_get_int_val(cfg, "icomsimulateautoinfo", 0) << 0;
		c |= cfg_get_int_val(cfg, "digitalovervoicerule", 0) << 1;
		c |= cfg_get_int_val(cfg, "usedecoderifconnected", 0) << 3;
		c |= cfg_get_int_val(cfg, "dontinterfereusbcontrol", 0) << 3;
		buf_append_c(&buf, c);
	}
	buf_append_c(&buf, cmd | MSB_BIT);

	mhr_set_bps_limit(ctl->router, channel, fbaud);

	submit_cmd(ctl, &buf, cb, user_data);

	// atl_warn_unused(args, "(mhc) set channel");

	return 0;
}

int mhc_set_speed(struct mh_control *ctl, int channel, struct cfg *cfg, mhc_cmd_completion_cb cb, void *user_data) {

	dbg1("(mhc) %s %s()",ctl->serial, __func__);

	if(channel < 0 || channel >= MH_NUM_CHANNELS) {
		err("(mhc) can't set speed, invalid channel (%d) specified!", channel);
		return -1;
	}

	// baud
	float fbaud = cfg_get_float_val(cfg,"baud", -1);
	if(fbaud == -1) {
		err("(mhc) can't set speed, no baud rate specified!");
		return -1;
	}

	if(ctl->speed_args)
		cfg_destroy(ctl->speed_args[channel]);
	ctl->speed_args[channel] = cfg_copy(cfg);

	int r = 0;

	if(mhc_is_online(ctl))
		r = submit_speed_cmd(ctl, channel, cb, user_data);
	else
		if(cb)
			cb((uint8_t*)"", 0, CMD_RESULT_OK, user_data);

	return r;

}

int mhc_set_mode(struct mh_control *ctl, int mode, mhc_cmd_completion_cb cb, void *user_data) {
	if(mhc_is_online(ctl)) {
		struct buffer b;
		buf_reset(&b);
		buf_append_c(&b, MHCMD_SET_KEYER_MODE);
		buf_append_c(&b, mode);
		buf_append_c(&b, MHCMD_SET_KEYER_MODE | MSB_BIT);
		return submit_cmd(ctl, &b, cb, user_data);
	}
	
	ctl->set_mode = mode;
	return 0;
}

int mhc_load_kopts(struct mh_control *ctl, mhc_cmd_completion_cb cb, void *user_data) {
	if(!mhc_is_online(ctl)) {
		return -1;
	}

	struct buffer *kb = kcfg_get_buffer(ctl->kcfg);
	struct buffer buf;
	buf_reset(&buf);
	buf_append_c(&buf, MHCMD_SET_SETTINGS);
	buf_append(&buf, kb->data, kb->size);
	buf_append_c(&buf, MHCMD_SET_SETTINGS | MSB_BIT);
	return submit_cmd(ctl, &buf, cb, user_data);
}

int mhc_mk2r_set_hfocus(struct mh_control *ctl, uint8_t hfocus[8], mhc_cmd_completion_cb cb, void *user_data) {
	struct buffer b;
	buf_reset(&b);
	buf_append_c(&b, MHCMD_HOST_FOCUS_CONTROL);
	buf_append(&b, hfocus, 8);
	buf_append_c(&b, MHCMD_HOST_FOCUS_CONTROL | MSB_BIT);
	memcpy(ctl->hfocus, hfocus, 8);
	return submit_cmd(ctl, &b, cb, user_data);
}

int mhc_mk2r_get_hfocus(struct mh_control *ctl, uint8_t dest[8]) {
	memcpy(dest, ctl->hfocus, 8);
	return 0;
}

int mhc_mk2r_set_scenario(struct mh_control *ctl, uint8_t idx, mhc_cmd_completion_cb cb, void *user_data) {
	struct buffer b;
	buf_reset(&b);
	buf_append_c(&b, MHCMD_APPLY_SCENARIO);
	buf_append_c(&b, idx);
	buf_append_c(&b, MHCMD_APPLY_SCENARIO | MSB_BIT);
	return submit_cmd(ctl, &b, cb, user_data);
}

int mhc_set_kopt(struct mh_control *ctl, const char *key, int val) {
	if(!key || !*key) {
		warn("(mhc) %s() nameless config item!", __func__);
		return -1;
	}

	if(ctl->mhi.type == MHT_UNKNOWN ||ctl->kcfg == NULL) {
		warn("(mhc) can't set parameter %s, keyer type not known!", key);
		return -1;
	}

	if(kcfg_set_val(ctl->kcfg, key, val)) {
		err("(mhc) could not set keyer parameter %s=%d!", key, val);
		return -1;
	}

	dbg0("(mhc) %s set keyer option %s=%d", ctl->serial, key, val);

	return 0;
}

#if 0
int mhc_set_kopts(struct mh_control *ctl, struct cfg *cfg) {
	int rval = 0;

	if(ctl->mhi.type == MHT_UNKNOWN ||ctl->kcfg == NULL) {
		warn("(mhc) can't set parameter list, keyer type not known!");
		return -1;
	}

	struct cfg *iter = cfg_child(cfg);
	if(!iter)
		return 0;

	do {
		const char *key = cfg_name(iter);

		if(!key || !*key) {
			warn("(mhc) %s nameless cfg item!", __func__);
			rval++;
			continue;
		}

		int val = cfg_get_int_val(iter, key, -1);
		int err = kcfg_set_val(ctl->kcfg, key, val);
		if(err) {
			err("(mhc) could not set keyer parameter %s=%d!", key, val);
			rval++;
			continue;
		}

		// dbg0("(mhc) set keyer option %s=%d", key, val);

	} while((iter = cfg_next(iter)));

	return rval;
}
#endif

int mhc_kopts_to_cfg(struct mh_control *ctl, struct cfg *cfg) {
	struct kcfg_iterator iter;
	const char *key;
	int val;

	if(!ctl || !ctl->kcfg) {
		//		err("(mhc) %s %p %p", __func__, ctl, ctl ? ctl->kcfg : NULL);
		return -1;
	}

	kcfg_iter_begin(ctl->kcfg, &iter);
	while(kcfg_iter_next(&iter)) {
		if(!kcfg_iter_get(&iter, &key, &val))
			cfg_set_int_value(cfg, key, val);
		else
			err("(mhc) %s() iterator error", __func__);
	}

	return 0;
}

uint16_t mhc_get_type(struct mh_control *ctl) {
	return ctl->mhi.type;
}

const struct cfg *mhc_get_speed_args(struct mh_control *ctl, int channel) {
	if(channel < 0 || channel >= MH_NUM_CHANNELS)
		return NULL;
	return ctl->speed_args[channel];
}

void mhc_add_state_changed_cb(struct mh_control *ctl, mhc_state_changed_cb cb, void *user_data) {
	if(!ctl)
		return;

	struct state_changed_cb *sccb;
	sccb = w_malloc(sizeof(*sccb));
	sccb->cb = cb;
	sccb->user_data = user_data;
	PG_AddTail(&ctl->state_changed_cb_list, &sccb->node);

	sccb->cb(ctl->serial, ctl->keyer_state, sccb->user_data);
}

void mhc_rem_state_changed_cb(struct mh_control *ctl, mhc_state_changed_cb cb) {
	struct state_changed_cb *sccb;
	PG_SCANLIST(&ctl->state_changed_cb_list, sccb) {
		if(sccb->cb == cb) {
			PG_Remove(&sccb->node);
			free(sccb);
			return;
		}
	}
	warn("(mhc) %s() callback not found!", __func__);
}

static int push_cmds(struct mh_control *ctl) {
	struct command *cmd = (void*)PG_FIRSTENTRY(&ctl->cmd_list);
	if(cmd == NULL)
		return 0;
	if(cmd->state == CMD_STATE_SENT)
		return 0;
#if 1
	int r = mhr_send(ctl->router, cmd->cmd, cmd->len, MH_CHANNEL_CONTROL);
	dbg1_h("(mhc) cmd to k: ", cmd->cmd, cmd->len);
	if(r != cmd->len) {
		warn("(mhc) could not send command! (%d/%d)", r, cmd->len);
		return -1;
	}
#endif
	ev_timer_set(&ctl->cmd_timeout_timer, CMD_TIMEOUT, 0.);
	ev_timer_start(ctl->loop, &ctl->cmd_timeout_timer);
	cmd->state = CMD_STATE_SENT;
	return 0;
}

static int submit_cmd_simple(struct mh_control *ctl, int cmd, mhc_cmd_completion_cb cb, void *user_data) {
	struct buffer buf;
	buf_reset(&buf);
	buf_append_c(&buf, cmd);
	buf_append_c(&buf, cmd | MSB_BIT);
	return submit_cmd(ctl, &buf, cb, user_data);
}

static int submit_cmd(struct mh_control *ctl, struct buffer *b, mhc_cmd_completion_cb cb, void *user_data) {

	if(b->size > MAX_CMD_LEN) {
		err("(mhc) Can't queue command, command too long (%d)!", b->size);
		return -1;
	}

	if(PG_Count(&ctl->cmd_list) > MAX_CMD_QUEUE_SIZE) {
		warn("(mhc) Can't queue command, queue full!");
		return -1;
	}

	struct command *cmd;
	cmd = (void*)PG_FIRSTENTRY(&ctl->free_list);
	if(cmd != NULL)
		PG_Remove(&cmd->node);
	else
		cmd = w_malloc(sizeof(*cmd));

	cmd->state = CMD_STATE_QUEUED;
	cmd->cmd_completion_cb = cb ? cb : generic_cb;
	cmd->user_data = user_data;
	memcpy(cmd->cmd, b->data, b->size);
	cmd->len = b->size;

	PG_AddTail(&ctl->cmd_list, &cmd->node);

	push_cmds(ctl);

	return 0;
}

const char *mhc_cmd_err_string(int result) {
	switch(result) {
	case CMD_RESULT_INVALID:
		return "invalid command result";
	case CMD_RESULT_OK:
		return "ok";
	case CMD_RESULT_TIMEOUT:
		return "command timed out";
	case CMD_RESULT_NOT_SUPPORTED:
		return "command not supported by keyer";
	}
	return "unkown command result";
}
