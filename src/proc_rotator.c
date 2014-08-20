#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "proc_rotator.h"
#include "util.h"
#include "buffer.h"
#include "logger.h"
#include "mhcontrol.h"

#define ROT_MAX_CMD_SIZE (12)
#define INVALID_BEARING (UINT16_MAX)

struct proc_rotator {
        struct mh_control *ctl;
	char cmd[ROT_MAX_CMD_SIZE + 1];
        uint8_t cmd_len;
        unsigned cmd_overflow;
	uint16_t bearing;
        const char *action_name;
};

struct proc_rotator *rot_create(struct mh_control *ctl) {
	struct proc_rotator *rot;
        rot = w_calloc(1, sizeof(*rot));
        rot->ctl = ctl;
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
                err("(rot) %s command failed: %s!", rot->action_name, mhc_cmd_err_string(result));
                return;
        }
        dbg1("(rot) %s cmd ok", rot->action_name);
}

static int process_cmd(struct proc_rotator *rot) {

	if(rot->cmd_len < 3 || rot->cmd[rot->cmd_len - 1] != ';') {
		err("(rot) Invalid command: %s!", rot->cmd);
		return -1;
	}

	dbg1("(rot) command: %s", rot->cmd);

	if(!strncmp(rot->cmd, "API", 3)) {
		if(strlen(rot->cmd) != 7) {
			err("(rot) Invalid parameters for API command!");
			return -1;
		}
		int bearing = atoi(rot->cmd + 3);
		if(bearing < 0 || bearing > 359) {
			err("(rot) Invalid bearing value for API command!");
			return -1;
		}

		rot->bearing = bearing;
		return 0;
	}

	if(!strcmp(rot->cmd, "AM 1;")) {
		if(rot->bearing == INVALID_BEARING) {
			err("(rot) Can't execute AM 1; command, bearing not set!");
			return -1;
		}
		rot->action_name = "TURN TO AZIMUTH";
		return mhc_sm_turn_to_azimuth(rot->ctl, rot->bearing, completion_cb, rot);
	}


	err("(rot) Invalid command: %s!", rot->cmd);
	return -1;
}


void rot_cb(struct mh_router *router, int channel, struct buffer *b, int fd, void *user_data) {
	(void)router; (void)channel; (void)fd;
	struct proc_rotator *rot = user_data;
        int c;

	dbg1("(rot) %s()", __func__);

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
				err("(rot) error processing command: %s", rot->cmd);
                        }
			rot->cmd_len = 0;
                        continue;
                }

		if(rot->cmd_len >= ROT_MAX_CMD_SIZE) {
			rot->cmd_overflow = 1;
			err("(rot) command too long: %s(...)", rot->cmd);
                        continue;
                }

		rot->cmd[rot->cmd_len++] = c;
        }

        buf_reset(b);
}

