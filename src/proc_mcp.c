/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include "proc_mcp.h"
#include "util.h"
#include "buffer.h"
#include "logger.h"
#include "mhcontrol.h"
#include "mhmk2r.h"

#define MCP_MAX_CMD_SIZE (32)

struct proc_mcp {
	struct mh_control *ctl;
	char cmd[MCP_MAX_CMD_SIZE + 1];
	uint8_t cmd_len;
	unsigned cmd_overflow;
	const char *action_name;
};

struct proc_mcp *mcp_create(struct mh_control *ctl) {
	struct proc_mcp *mcp;
	mcp = w_calloc(1, sizeof(*mcp));
	mcp->ctl = ctl;
	return mcp;
}

void mcp_destroy(struct proc_mcp *mcp) {
	free(mcp);
}

static void completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data)  {
	(void)reply_buf; (void)len;
	struct proc_mcp *mcp = user_data;

	if(result != CMD_RESULT_OK) {
		err("(mcp) %s command failed: %s!", mcp->action_name, mhc_cmd_err_string(result));
		return;
	}
	dbg1("(mcp) %s cmd ok :)", (const char *)user_data);
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

	mk2r_set_hfocus_value(hfocus, "directControl", 0);

	return 0;
}

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

	if(mcp->cmd_len < 2)
		return -1;

	dbg1("(mcp) command: %s", mcp->cmd);

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

	if(!strncmp(mcp->cmd, "SA", 2) && strlen(mcp->cmd) == 3) {
		char arg[2];
		arg[0] = mcp->cmd[2];
		arg[1] = 0;
		if(arg[0] >= '0' && arg[0] <= '7') {
			mcp->action_name = "APPLY SCENARIO";
			return mhc_mk2r_set_scenario(mcp->ctl, atoi(arg), completion_cb, mcp);
		}
	}

	err("(mcp) invalid command: %s", mcp->cmd);

	return -1;

set_hfocus:
	mcp->action_name = "HOST FOCUS";
	return mhc_mk2r_set_hfocus(mcp->ctl, hfocus, completion_cb, mcp);

}

void mcp_cb(struct mh_router *router, int channel, struct buffer *b, void *user_data) {
	(void)router; (void)channel;
	struct proc_mcp *mcp = user_data;
	int c;

	dbg1("(mcp) %s()", __func__);

	while(-1 != (c = buf_get_c(b))) {

		if(mcp->cmd_overflow) {
			if(c == 0x0d || c == 0x0a) {
				mcp->cmd_overflow = 0;
				mcp->cmd_len = 0;
			}
			continue;
		}

		if(c == 0x0d || c == 0x0a) {
			if(!mcp->cmd_len)
				continue;

			mcp->cmd[mcp->cmd_len] = 0;
			if(-1 == process_cmd(mcp)) {
				err("(mcp) error processing command: %s", mcp->cmd);
			}
			mcp->cmd_len = 0;
			continue;
		}

		if(mcp->cmd_len >= MCP_MAX_CMD_SIZE) {
			mcp->cmd_overflow = 1;
			err("(mcp) command too long: %s(...)", mcp->cmd);
			continue;
		}

		mcp->cmd[mcp->cmd_len++] = c;
	}

	buf_reset(b);
}

