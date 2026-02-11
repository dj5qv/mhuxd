/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "proc_rotator.h"
#include "util.h"
#include "buffer.h"
#include "logger.h"
#include "mhcontrol.h"

#define MOD_ID "rot"

#define ROT_MAX_CMD_SIZE (12)
#define INVALID_BEARING (UINT16_MAX)

struct proc_rotator {
	struct mh_control *ctl;
	int channel;
	char cmd[ROT_MAX_CMD_SIZE + 1];
	uint8_t cmd_len;
	unsigned cmd_overflow;
	uint16_t bearing;
	const char *action_name;
};

struct proc_rotator *rot_create(struct mh_control *ctl, int channel){
	struct proc_rotator *rot;
        rot = w_calloc(1, sizeof(*rot));
        rot->ctl = ctl;
	rot->channel = channel;
	rot->bearing = INVALID_BEARING;
        return rot;
}

void rot_destroy(struct proc_rotator *rot) {
	free(rot);
}

static void completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data)  {
        (void)reply_buf; (void)len;
	struct proc_rotator *rot = user_data;

        if(result != CMD_RESULT_OK) {
                err("%s command failed: %s!", rot->action_name, mhc_cmd_err_string(result));
                return;
        }
        dbg1("%s cmd ok", rot->action_name);
}

static int process_cmd(struct proc_rotator *rot) {

	if(rot->cmd_len < 3 || rot->cmd[rot->cmd_len - 1] != ';') {
		err("Invalid command: %s!", rot->cmd);
		return -1;
	}

	dbg1("command: %s", rot->cmd);

	if(!strncmp(rot->cmd, "API", 3)) {
		if(strlen(rot->cmd) != 7) {
			err("Invalid parameters for API command!");
			return -1;
		}
		int bearing = atoi(rot->cmd + 3);
		if(bearing < 0 || bearing > 359) {
			err("Invalid bearing value for API command!");
			return -1;
		}

		rot->bearing = bearing;
		return 0;
	}

	if(!strcmp(rot->cmd, "AM 1;")) {
		if(rot->bearing == INVALID_BEARING) {
			err("Can't execute AM 1; command, bearing not set!");
			return -1;
		}
		rot->action_name = "TURN TO AZIMUTH";
		mhc_sm_turn_to_azimuth(rot->ctl, rot->bearing, completion_cb, rot);
		return 0;
	}


	err("Invalid command: %s!", rot->cmd);
	return -1;
}


void rot_cb(struct mh_router *router, struct buffer *b, void *user_data) {
	(void)router;
	struct proc_rotator *rot = user_data;
        int c;

	dbg1("%s()", __func__);

        while(-1 != (c = buf_get_c(b))) {

		if(rot->cmd_overflow) {
                        if(c == 0x0d || c == 0x0a) {
				rot->cmd_overflow = 0;
				rot->cmd_len = 0;
                        }
                        continue;
                }

                if(c == 0x0d || c == 0x0a) {
			if(!rot->cmd_len)
                                continue;

			rot->cmd[rot->cmd_len] = 0;
			if(-1 == process_cmd(rot)) {
				err("error processing command: %s", rot->cmd);
                        }
			rot->cmd_len = 0;
                        continue;
                }

		if(rot->cmd_len >= ROT_MAX_CMD_SIZE) {
			rot->cmd_overflow = 1;
			err("command too long: %s(...)", rot->cmd);
                        continue;
                }

		rot->cmd[rot->cmd_len++] = c;
        }

        buf_reset(b);
}

