/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include "util.h"
#include "buffer.h"
#include "logger.h"
#include "mhcontrol.h"

#define MCP_MAX_CMD_SIZE (32)

struct proc_mcp {
	struct mh_control *ctl;
	char cmd[MCP_MAX_CMD_SIZE + 1];
	uint8_t cmd_len;
	unsigned cmd_overflow;
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

static void process_cmd(struct proc_mcp *mcp) {
	if(!mcp->cmd_len)
		return;
}

void mcp_cb(struct mh_router *router, int channel, struct buffer *b, void *user_data) {
	(void)router; (void)channel;
	struct proc_mcp *mcp = user_data;
	int c;

	dbg1("(mhr) %s()", __func__);

	while(-1 != (c = buf_get_c(b))) {

		if(mcp->cmd_overflow) {
			if(c == 0x0d || c == 0x0a) {
				mcp->cmd_overflow = 0;
				mcp->cmd_len = 0;
			}
			continue;
		}

		if(c == 0x0d || c == 0x0a) {
			process_cmd(mcp);
			continue;
		}

		if(mcp->cmd_len >= MCP_MAX_CMD_SIZE) {
			mcp->cmd_overflow = 1;
			err("(mcp) command too long (%s)", mcp->cmd);
			continue;
		}

		mcp->cmd[mcp->cmd_len++] = c;
	}

	buf_reset(b);
}

