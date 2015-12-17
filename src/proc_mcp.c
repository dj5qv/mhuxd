/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2016  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "proc_mcp.h"
#include "util.h"
#include "buffer.h"
#include "logger.h"
#include "mhcontrol.h"
#include "mhmk2r.h"
#include "mhinfo.h"
#include "mhrouter.h"
#include "mhflags.h"
#include "channel.h"

#define MOD_ID "mcp"

#define MCP_MAX_CMD_SIZE (32)

static void flags_cb(struct mh_router *router, unsigned const char *data ,int len, int channel, void *user_data);
static void mok_state_changed_cb(const char *serial, const uint8_t *state, uint8_t state_len, void *user_data);
static void acc_state_changed_cb(const char *serial, const uint8_t *state, uint8_t state_len, void *user_data);
void mode_changed_cb(const char *serial, uint8_t mode_cur, uint8_t mode_r1, uint8_t mode_r2, void *user_data);

struct proc_mcp {
	struct mh_control *ctl;
	char cmd[MCP_MAX_CMD_SIZE + 1];
	uint8_t cmd_len;
	unsigned cmd_overflow;
	const char *action_name;
	struct mhc_mode_callback *mcb;
	struct mhc_state_callback *acccb, *mokcb;
	int fd;
	uint8_t flag[2];
	uint8_t mok_state[8];
	uint8_t acc_state[4];
	int auto_info_on : 1;
};

struct proc_mcp *mcp_create(struct mh_control *ctl) {
	struct proc_mcp *mcp;
	dbg1("%s()", __func__);
	mcp = w_calloc(1, sizeof(*mcp));
	mcp->ctl = ctl;

	mhr_add_consumer_cb(mhc_get_router(mcp->ctl), flags_cb, MH_CHANNEL_FLAGS, mcp);
	mcp->mokcb = mhc_add_mok_state_changed_cb(mcp->ctl, mok_state_changed_cb, mcp);
	mcp->acccb = mhc_add_acc_state_changed_cb(mcp->ctl, acc_state_changed_cb, mcp);
	mcp->mcb = mhc_add_mode_changed_cb(mcp->ctl, mode_changed_cb, mcp);

	return mcp;
}

void mcp_destroy(struct proc_mcp *mcp) {
	dbg1("%s()", __func__);
	mhc_rem_mode_changed_cb(mcp->ctl, mcp->mcb);
	mhc_rem_acc_state_changed_cb(mcp->ctl, mcp->acccb);
	mhc_rem_mok_state_changed_cb(mcp->ctl, mcp->mokcb);
	mhr_rem_consumer_cb(mhc_get_router(mcp->ctl), flags_cb, MH_CHANNEL_FLAGS);
	free(mcp);
}

static void send_response(int fd, const char *cmd, const char *arg) {
	ssize_t r;
	char response[MCP_MAX_CMD_SIZE + 3];
	snprintf(response, sizeof(response), "%s%s\r", cmd, arg);

	dbg1("%s(): %s", __func__, response);

	r = write(fd, response, strlen(response));
	if(r <= 0)
		err_e(errno, "%s() could not write response!", __func__);
}

static void send_response_int(int fd, const char *cmd, int32_t iarg) {
	char arg[12];
	snprintf(arg, sizeof(arg), "%d", iarg);
	send_response(fd, cmd, arg);
}

static void send_err_response(int fd, const char *cmd) {
	ssize_t r;
	char response[MCP_MAX_CMD_SIZE + 4];
	*response = 'E';
	strcpy(response + 1, cmd);
	strcat(response, "\r");

	dbg1("%s(): %s", __func__, response);

	r = write(fd, response, strlen(response));
	if(r <= 0)
		err_e(errno, "%s() could not write response!", __func__);
}

static void completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data)  {
	(void)reply_buf; (void)len;
	struct proc_mcp *mcp = user_data;

	if(result != CMD_RESULT_OK) {
		err("%s command failed: %s!", mcp->action_name, mhc_cmd_err_string(result));
		send_err_response(mcp->fd, mcp->cmd);
		return;
	}
	dbg1("%s cmd ok", mcp->action_name);
}

static int frd_to_hfocus(uint8_t hfocus[8], const char *frd_arg) {
	int i;

	if(strlen(frd_arg) != 12)
		return -1;

	for(i = 0; i < 12; i++)
		if(frd_arg[i] != '0' && frd_arg[i] != '1')
			return -1;

	i = 0;

	mk2r_set_hfocus_value(hfocus, "ears.left.r1Main", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.left.r1Sub", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.left.scLeft", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.left.scRight", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.left.r2Main", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.left.r2Sub", frd_arg[i++] == '1');

	mk2r_set_hfocus_value(hfocus, "ears.right.r1Main", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.right.r1Sub", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.right.scLeft", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.right.scRight", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.right.r2Main", frd_arg[i++] == '1');
	mk2r_set_hfocus_value(hfocus, "ears.right.r2Sub", frd_arg[i++] == '1');

	mk2r_set_hfocus_value(hfocus, "directControl", 1);

	return 0;
}

static int cmd_am(struct proc_mcp *mcp) {
	uint8_t acc_outputs[4];
	uint8_t offset = (mcp->cmd[2] == '2' ? 2 : 0);
	int i;
	uint16_t acc_int;

	mhc_mk2r_get_acc_outputs(mcp->ctl, acc_outputs);

	if(strlen(mcp->cmd) == 3) {
		// query
		char arg[16 + 1];
		char *p = arg;
		arg[16] = 0;
		acc_int = acc_outputs[offset] << 8 | acc_outputs[offset + 1];

		for(i = 0; i < 16; i++) {
			*p++ = ((acc_int >> (15 - i)) & 1) ? '1' : '0';
		}
		send_response(mcp->fd, mcp->cmd, arg);
		return 0;
	}

	if(strlen(mcp->cmd) == 19) {
		// set
		char *p = mcp->cmd + 3;
		acc_int = 0;

		for(i = 0; i < 16; i++) {
			switch(p[i]) {
			case '0':
				break;
			case '1':
				acc_int |= (acc_int | 1 << (15 - i));
				break;
			default:
				err("%s Invalid parameter!", mcp->cmd);
				return -1;
				break;

			}
		}

		acc_outputs[offset] = acc_int >> 8;
		acc_outputs[offset + 1] = acc_int & 0xff;

		return mhc_mk2r_set_acc_outputs(mcp->ctl, acc_outputs, completion_cb, mcp);
	}

	err("%s Invalid parameter!", mcp->cmd);
	return -1;
}

static int cmd_ver(struct proc_mcp *mcp) {
	char c = mcp->cmd[1];

	if(c == 'S' || c == 0) {
		send_response(mcp->fd, "VS", mhc_get_serial(mcp->ctl));
	}
	if(c == 'F' || c == 0) {
		const struct mh_info *mhi = mhc_get_mhinfo(mcp->ctl);
		char arg[10];
		snprintf(arg, sizeof(arg), "%02d.%02d", mhi->ver_fw_major, mhi->ver_fw_minor);
		send_response(mcp->fd, "VF", arg);
	}
	if(c == 'R' || c == 0) {
		char *arg = "08.05.06";
		send_response(mcp->fd, "VR", arg);
	}

	if(c == 0) {
		// Though not so in the specs, Windows router seems to add connection status here.
		char *arg = mhc_is_online(mcp->ctl) ? "1" : "0";
		send_response(mcp->fd, "C", arg);
	}

	return 0;
}

//static int am_to_acc(uint8_t acc[16]

/*
 * FT1<CR> SetTxFocus(R1)
 * FT2<CR> SetTxFocus(R2)
 * FR1<CR> SetRxFocus(R1)
 * FR2<CR> SetRxFocus(R2)
 * FRS<CR> SetRxFocus(STEREO)
 * FRDxxxxxxxxxxxx<CR> SetRxFocus(DIRECT)
 * AM1xxxxxxxxxxxxxxxx<CR> SetAccOutputs(R1, outputs) -> TBC
 * AM2xxxxxxxxxxxxxxxx<CR> SetAccOutputs(R2, outputs) -> TBC
 * AS1dd<CR> SetAccOutputSelection(R1, selection) -> TBC
 * AS2dd<CR> SetAccOutputSelection(R2, selection) -> TBC
 * SAs<CR> ApplyScenario(scenarioIndex) -> mhc/APPLY SCENARIO
 * MA<CR> AbortMessage() -> mhc/ABORT CW/FSK MESSAGE
 * MPm<CR> PlayMessage(msgIndex) -> mhc/PLAY CW/FSK MESSAGE
 * MPImi<CR> PlayMessagePeriodically(msgIndex, interval) -> TBC, probably to be implemented in router
 * MRm<CR> StartMessageRecording(msgIndex) -> mhc/RECORD CW/FSK MESSAGE
 * MRS<CR> StopMessageRecording() -> mhc/RECORD CW/FSK MESSAGE
 * MBname<CR> SetMessageBank(msgName) -> TBC, probably implemented in router
 *
 */
static int process_cmd(struct proc_mcp *mcp) {
	uint8_t hfocus[8];

	if(mcp->cmd_len < 1)
		return -1;

	dbg1("command: %s", mcp->cmd);

	mhc_mk2r_get_hfocus(mcp->ctl, hfocus);

	if(!strcmp(mcp->cmd, "FT1")) {
		mk2r_set_hfocus_value(hfocus, "txFocus", 0);
		goto set_hfocus;
	}

	if(!strcmp(mcp->cmd, "FT2")) {
		mk2r_set_hfocus_value(hfocus, "txFocus", 1);
		goto set_hfocus;
	}

	if(!strcmp(mcp->cmd, "FR1")) {
		mk2r_set_hfocus_value(hfocus, "rxFocus", 0);
		mk2r_set_hfocus_value(hfocus, "stereoFocus", 0);
		mk2r_set_hfocus_value(hfocus, "directControl", 0);
		goto set_hfocus;
	}

	if(!strcmp(mcp->cmd, "FR2")) {
		mk2r_set_hfocus_value(hfocus, "rxFocus", 1);
		mk2r_set_hfocus_value(hfocus, "stereoFocus", 0);
		mk2r_set_hfocus_value(hfocus, "directControl", 0);
		goto set_hfocus;
	}

	if(!strcmp(mcp->cmd, "FRS")) {
		mk2r_set_hfocus_value(hfocus, "stereoFocus", 1);
		mk2r_set_hfocus_value(hfocus, "directControl", 0);
		goto set_hfocus;
	}

	if(!strncmp(mcp->cmd, "FRD", 3)) {
		if(0 == frd_to_hfocus(hfocus, mcp->cmd + 3))
			goto set_hfocus;
	}

	if(!strncmp(mcp->cmd, "AM1", 3) || !strncmp(mcp->cmd, "AM2", 3)) {
		return cmd_am(mcp);
	}

	if(!strcmp(mcp->cmd,"VS") || !strcmp(mcp->cmd,"VF") || !strcmp(mcp->cmd,"VR") || !strcmp(mcp->cmd,"V")) {
		return cmd_ver(mcp);
	}

	if(!strcmp(mcp->cmd, "I0")) {
		mcp->auto_info_on = 0;
		return 0;
	}

	if(!strcmp(mcp->cmd, "I1")) {
		mcp->auto_info_on = 1;
		return 0;
	}

	if(!strcmp(mcp->cmd, "C")) {
		char *arg = mhc_is_online(mcp->ctl) ? "1" : "0";
		send_response(mcp->fd, "C", arg);
		return 0;
	}
	if(!strncmp(mcp->cmd, "SA", 2) && strlen(mcp->cmd) == 3) {
		char arg[2];
		arg[0] = mcp->cmd[2];
		arg[1] = 0;
		if(arg[0] >= '0' && arg[0] <= '7') {
			mcp->action_name = "APPLY SCENARIO";
			return mhc_mk2r_set_scenario(mcp->ctl, atoi(arg), completion_cb, mcp);
		}
	}

	if(!strncmp(mcp->cmd, "MR", 2) && strlen(mcp->cmd) == 3) {
		char arg[2];
		arg[0] = mcp->cmd[2];
		arg[1] = 0;
		if(arg[0] >= '1' && arg[0] <= '9') {
			mcp->action_name = "RECORD MESSAGE";
			return mhc_record_message(mcp->ctl, atoi(arg), completion_cb, mcp);
		}
	}

	if(!strncmp(mcp->cmd, "MP", 2) && strlen(mcp->cmd) == 3) {
		char arg[2];
		arg[0] = mcp->cmd[2];
		arg[1] = 0;
		if(arg[0] >= '1' && arg[0] <= '9') {
			mcp->action_name = "PLAY MESSAGE";
			return mhc_play_message(mcp->ctl, atoi(arg), completion_cb, mcp);
		}
	}

	if(!strcmp(mcp->cmd, "MRS")) {
		mcp->action_name = "STOP RECORDING";
		return mhc_stop_recording(mcp->ctl, completion_cb, mcp);
	}

	if(!strcmp(mcp->cmd, "MA")) {
		mcp->action_name = "ABORT MESSAGE";
		return mhc_abort_message(mcp->ctl, completion_cb, mcp);
	}



	err("invalid command: %s", mcp->cmd);

	return -1;

set_hfocus:
	mcp->action_name = "HOST FOCUS";
	return mhc_mk2r_set_hfocus(mcp->ctl, hfocus, completion_cb, mcp);

}

void mcp_cb(struct mh_router *router, int channel, struct buffer *b, int fd, void *user_data) {
	(void)router; (void)channel;
	struct proc_mcp *mcp = user_data;
	int c;

	dbg1("%s()", __func__);

	mcp->fd = fd;

	while(-1 != (c = buf_get_c(b))) {

		if(mcp->cmd_overflow) {
			if(c == 0x0d || c == 0x0a) {
				mcp->cmd_overflow = 0;
				mcp->cmd_len = 0;
				send_err_response(fd, mcp->cmd);
			}
			continue;
		}

		if(c == 0x0d || c == 0x0a) {
			if(!mcp->cmd_len)
				continue;

			mcp->cmd[mcp->cmd_len] = 0;
			if(-1 == process_cmd(mcp)) {
				err("error processing command: %s", mcp->cmd);
				send_err_response(fd, mcp->cmd);
			}
			mcp->cmd_len = 0;
			continue;
		}

		if(mcp->cmd_len >= MCP_MAX_CMD_SIZE) {
			mcp->cmd_overflow = 1;
			err("command too long: %s(...)", mcp->cmd);
			continue;
		}

		mcp->cmd[mcp->cmd_len++] = c;
	}

	buf_reset(b);
}

static void flags_cb(struct mh_router *router, unsigned const char *data ,int len, int channel, void *user_data) {
	(void) router; (void) channel;
	struct proc_mcp *mcp = user_data;
	uint16_t i;
	uint8_t ptt_changed = 0, footsw_changed = 0, lockout_changed = 0;

	dbg1("%s()", __func__);
	
	for(i = 0; i < len; i++) {
		uint8_t idx = 0;
		
		if(data[i] & MHD2CFL_R2) 
			idx = 1;
			
		if((mcp->flag[idx] & MHD2CFL_ANY_PTT) != (data[i] & MHD2CFL_ANY_PTT)) 
			ptt_changed = 1;

		if((mcp->flag[idx] & MHD2CFL_FOOTSWITCH) != (data[i] & MHD2CFL_FOOTSWITCH))
			footsw_changed = 1;

		if((mcp->flag[idx] & MHD2CFL_LOCKOUT) != (data[i] & MHD2CFL_LOCKOUT))
			lockout_changed = 1;

		
		mcp->flag[idx] = data[i];
	}

	if(mcp->auto_info_on && ptt_changed) {
		uint16_t type = mhc_get_mhinfo(mcp->ctl)->type;
		char arg[4];
		if(type == MHT_MK2R || type == MHT_MK2Rp || type == MHT_U2R)
			snprintf(arg, sizeof(arg), "%d%d\r",
				 (mcp->flag[0] & MHD2CFL_ANY_PTT) ? 1 : 0,
				 (mcp->flag[1] & MHD2CFL_ANY_PTT) ? 1 : 0);
		else
			snprintf(arg, sizeof(arg), "%d\r",
				 (mcp->flag[0] & MHD2CFL_ANY_PTT) ? 1 : 0);
		send_response(mcp->fd, "P", arg);
	}

	if(mcp->auto_info_on && footsw_changed) {
		uint16_t type = mhc_get_mhinfo(mcp->ctl)->type;
		char arg[4];
		if(type == MHT_MK2R || type == MHT_MK2Rp || type == MHT_U2R)
			snprintf(arg, sizeof(arg), "%d%d\r",
				 (mcp->flag[0] & MHD2CFL_FOOTSWITCH) ? 1 : 0,
				 (mcp->flag[1] & MHD2CFL_FOOTSWITCH) ? 1 : 0);
		else
			snprintf(arg, sizeof(arg), "%d\r",
				 (mcp->flag[0] & MHD2CFL_FOOTSWITCH) ? 1 : 0);
		send_response(mcp->fd, "H", arg);
	}

	if(mcp->auto_info_on && lockout_changed) {
		uint16_t type = mhc_get_mhinfo(mcp->ctl)->type;
		char arg[4];
		if(type == MHT_MK2R || type == MHT_MK2Rp || type == MHT_U2R)
			snprintf(arg, sizeof(arg), "%d%d\r",
				 (mcp->flag[0] & MHD2CFL_LOCKOUT) ? 1 : 0,
				 (mcp->flag[1] & MHD2CFL_LOCKOUT) ? 1 : 0);
		else
			snprintf(arg, sizeof(arg), "%d\r",
				 (mcp->flag[0] & MHD2CFL_LOCKOUT) ? 1 : 0);
		send_response(mcp->fd, "L", arg);
	}

}

/*
 * Provide auto-information if requested (I1).
 * PTT info is handled by flag_cb().
 * ACC info is handled by acc_state_changed_cb()
 */
static void mok_state_changed_cb(const char *serial, const uint8_t *state, uint8_t state_len, void *user_data) {
	(void) serial;
	struct proc_mcp *mcp = user_data;
	
	dbg1("%s()", __func__);
	
	if(state_len < 8) {
		warn("MOK State length too short (%d)", state_len);
		return;
	}
	
	if(state_len > 8)
		state_len = 8;
	
	if(mcp->auto_info_on) {
		uint8_t val, rx_focus, rx_stereo;
		
		// FT
		val = mk2r_get_mok_value(state, "txFocus");
		if(val != mk2r_get_mok_value(mcp->mok_state, "txFocus"))
			send_response_int(mcp->fd, "FT", val + 1);

		// FTA
		val = mk2r_get_mok_value(state, "focusAuto");
		if(val != mk2r_get_mok_value(mcp->mok_state, "focusAuto"))
			send_response_int(mcp->fd, "FTA", val);

		// FR
		rx_focus = mk2r_get_mok_value(state, "rxFocus");
		rx_stereo= mk2r_get_mok_value(state, "stereoFocus");
		if((rx_focus != mk2r_get_mok_value(mcp->mok_state, "rxFocus")) || rx_stereo != mk2r_get_mok_value(mcp->mok_state, "stereoFocus")) {
			if(rx_stereo)
				send_response(mcp->fd, "FRS", "");
			else
				send_response_int(mcp->fd, "FR", val + 1);

		}

		// FRD
		if((mk2r_get_mok_value(state, "ears.left.flags") != mk2r_get_mok_value(mcp->mok_state, "ears.left.flags")) ||
		   (mk2r_get_mok_value(state, "ears.right.flags") != mk2r_get_mok_value(mcp->mok_state, "ears.right.flags"))) {
			char arg[13]; int i = 0;
			arg[i++] = mk2r_get_mok_value(state, "ears.left.r1Main") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.left.r1Sub") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.left.scLeft") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.left.scRight") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.left.r2Main") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.left.r2Sub") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.right.r1Main") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.right.r1Sub") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.right.scLeft") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.right.scRight") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.right.r2Main") ? '1' : '0';
			arg[i++] = mk2r_get_mok_value(state, "ears.right.r2Sub") ? '1' : '0';
			arg[i] = 0x00;
			send_response(mcp->fd, "FRD", arg);
		}
		
	}

	memcpy(mcp->mok_state, state, state_len);
}

static void acc_state_changed_cb(const char *serial, const uint8_t *state, uint8_t state_len, void *user_data) {
	(void) serial;
	struct proc_mcp *mcp = user_data;

	dbg1("%s()", __func__);

	if(state_len < 4) {
		warn("ACC State length too short (%d)", state_len);
		return;
	}

	if(state_len > 4)
		state_len = 4;

	char arg[17]; int i;

	if(state[0] != mcp->acc_state[0] || state[1] != mcp->acc_state[1]) {
		uint16_t val = (state[0] << 8) | state[1];
		for(i = 0; i < 16; i++) {
			arg[i] = val >> (15 - i) ? '1' : '0';
		}
		arg[i] = 0x00;
		send_response(mcp->fd, "AM1", arg);
	}

	if(state[2] != mcp->acc_state[2] || state[3] != mcp->acc_state[3]) {
		uint16_t val = (state[2] << 8) | state[3];
		for(i = 0; i < 16; i++) {
			arg[i] = val >> (15 - i) ? '1' : '0';
		}
		arg[i] = 0x00;
		send_response(mcp->fd, "AM2", arg);
	}

	memcpy(mcp->acc_state, state, state_len);
}

static const char *keyer_modes[] = {
	"C", "V", "F", "D",
};

void mode_changed_cb(const char *serial, uint8_t mode_cur, uint8_t mode_r1, uint8_t mode_r2, void *user_data) {
	(void) serial;
	struct proc_mcp *mcp = user_data;
	uint16_t type = mhc_get_mhinfo(mcp->ctl)->type;

	dbg1("%s()", __func__);

	if(type == MHT_MK2R || type == MHT_MK2Rp || type == MHT_U2R) {
		send_response(mcp->fd, "K1", keyer_modes[mode_r1]);
		send_response(mcp->fd, "K2", keyer_modes[mode_r2]);
	} else {
		send_response(mcp->fd, "K", keyer_modes[mode_cur]);
	}
}
