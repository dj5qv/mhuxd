/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>

#include "global.h"
#include "util.h"
#include "dispatcher.h"
#include "opts.h"
#include "logger.h"
#include "mhproto.h"
#include "demux.h"
#include "mux.h"
#include "ctlmsg.h"

#define KEYER_CHANNEL	 MH_CHANNEL_MAX
#define CTL_CHANNEL_OFF  (KEYER_CHANNEL + 1)
#define CTL_CHANNEL_CAT1 (MH_CHANNEL_R1 + CTL_CHANNEL_OFF)
#define CTL_CHANNEL_CAT2 (MH_CHANNEL_R2 + CTL_CHANNEL_OFF)
#define CTL_CHANNEL_PTT1 (MH_CHANNEL_MAX + CTL_CHANNEL_OFF + 0)
#define CTL_CHANNEL_PTT2 (MH_CHANNEL_MAX + CTL_CHANNEL_OFF + 1)
#define NUM_CHANNELS	 (CTL_CHANNEL_PTT2 + 1)

static int ch_read(struct dispatcher *dp, int channel);
static int ch_read_fsk(struct dispatcher *dp, int channel);
static int ch_read_keyer(struct dispatcher *dp, int channel);


struct io_channel {
	int      fd;
	struct buffer   rbuf;
	struct buffer   wbuf;
	int    (*read_func)(struct dispatcher *, int channel);
};

struct timer {
	struct PGNode node;
	struct dispatcher *dp;
	int	id;
	int	ival;
	int	single_shot;
	struct timeval tv;
	void	(*timeout)(struct timer *, void *userdata);
	void	*userdata;
};


struct dispatcher {
	struct io_channel	ch[NUM_CHANNELS];
	struct pollfd		fds[NUM_CHANNELS];
	int			error;
	struct PGList           requests;
	struct PGList		timers;
	struct dmx		*dmx;
	struct mx               *mx;
	int                     terminate;
	unsigned char           keyer_flag;
	int			dev_timeout;
	int			r1_fsk_busy;
	int			r2_fsk_busy;
}; 

static const char *ch_strs[] = {
        [MH_CHANNEL_FLAGS]      = "CH-FLAGS",
        [MH_CHANNEL_CONTROL]    = "CH-CONTR",
        [MH_CHANNEL_WINKEY]     = "CH_WKEY ",
        [MH_CHANNEL_PS2]        = "CH-PS2  ",
        [MH_CHANNEL_R1]         = "CH-CAT1 ",
        [MH_CHANNEL_R2]         = "CH-CAT2 ",
        [MH_CHANNEL_R1_FSK]     = "CH-FSK1 ",
        [MH_CHANNEL_R2_FSK]     = "CH-FSK2 ",
        [KEYER_CHANNEL]         = "CH-KEYER",
	[MH_CHANNEL_FLAGS + CTL_CHANNEL_OFF]	  = "VCTL-FLAGS",
	[MH_CHANNEL_CONTROL + CTL_CHANNEL_OFF]    = "VCTL-CONTR",
	[MH_CHANNEL_WINKEY + CTL_CHANNEL_OFF]     = "VCTL_WKEY ",
	[MH_CHANNEL_PS2 + CTL_CHANNEL_OFF]        = "VCTL-PS2  ",
	[MH_CHANNEL_R1 + CTL_CHANNEL_OFF]         = "VCTL-CAT1 ",
	[MH_CHANNEL_R2 + CTL_CHANNEL_OFF]         = "VCTL-CAT2 ",
	[MH_CHANNEL_R1_FSK + CTL_CHANNEL_OFF]     = "VCTL-FSK1 ",
	[MH_CHANNEL_R2_FSK + CTL_CHANNEL_OFF]     = "VCTL-FSK2 ",
	[CTL_CHANNEL_PTT1]			  = "VCTL-PTT1",
	[CTL_CHANNEL_PTT2]			  = "VCTL-PTT2",
};

int sig_term = 0;
int sig_hup = 0;
static void (*hup_handler)(void *) = NULL;
static void *hup_handler_data = NULL;

static void sig_handler(int i) {
	switch(i) {
	case SIGINT:
	case SIGTERM:
		sig_term = i;
		break;
	case SIGHUP:
		sig_hup = i;
		break;
	}
}
static const struct sigaction sa = {
	.sa_handler	= &sig_handler,
};


struct dispatcher *dp_create() {
	unsigned i;
	struct dispatcher *dp = w_calloc(1, sizeof(*dp));

	dp->dmx = dmx_create();
	dp->mx  = mx_create();

	for(i = 0; i < MH_CHANNEL_MAX; i++) {
		dp->mx->ibuf[i] = &dp->ch[i].rbuf;
	}

	for(i=0; i<ARRAY_SIZE(dp->ch); i++) {
		dp->ch[i].fd = -1;
		memset(&dp->ch[i].rbuf, 0, sizeof(dp->ch[i].rbuf));
		memset(&dp->ch[i].wbuf, 0, sizeof(dp->ch[i].rbuf));
		switch(i) {
		case MH_CHANNEL_R1_FSK:
		case MH_CHANNEL_R2_FSK:
			dp->ch[i].read_func = ch_read_fsk;
			break;
		case KEYER_CHANNEL:
			dp->ch[i].read_func = ch_read_keyer;
			break;
		default:
			dp->ch[i].read_func = ch_read;
			break;
		}
	}

	PG_NewList(&dp->requests);
	PG_NewList(&dp->timers);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);


	/* Send an initial zero to make the state clear. Keyer may have
	 * PTT keyed from a previous session and would immediatly start
	 * TX after receiving an ARE YOU THERE command.
	 */
	buf_append_c(&dp->ch[MH_CHANNEL_FLAGS].rbuf, 0);

	return dp;
}

void dp_destroy(struct dispatcher *dp) {
	unsigned i;

	if(!dp)
		return;

	if(dp->dmx)
		dmx_destroy(dp->dmx);

	if(dp->mx)
		mx_destroy(dp->mx);

	for(i=0; i<ARRAY_SIZE(dp->ch); i++) {
		if(dp->ch[i].fd != -1)
			close(dp->ch[i].fd);	/* ?????, Keyerchannel ??? */
	}
	free(dp);
}

static void request_timeout(struct timer *t, void *userdata) {
	struct cmd_request *req = userdata;
	req->state = CRS_TIMEOUT;
	PG_Remove(&req->node);

	dp_destroy_timer(t);
	req->timer = NULL;

	if(req->req_done)
		req->req_done(req);
}

int dp_submit_request(struct dispatcher *dp, struct cmd_request *req) {
	if(!dp || !req)
		return -1;

	if(dp->terminate) {
		req->state = CRS_ERROR;
		if(req->req_done)
			req->req_done(req);
		err("DSP Can't submit request, dispatcher in terminate state!");
		return -1;
	}

	if(PG_Contains(&dp->requests, &req->node)) {
		err("DSP Can't submit request since it is already queued!");
		return -1;
	}

	req->timer = dp_create_timer(dp, dp->dev_timeout, 1, &request_timeout, req);
	dp_submit_timer(dp, req->timer);

	req->state = CRS_NEW;
	buf_reset(&req->b_resp);
	dbg0_h("W CTLCMD  ", req->b_cmd.data, req->b_cmd.size);
	PG_AddTail(&dp->requests, &req->node);
	return 0;
}

/* Difference from - sub in milliseconds. */
static long time_sub(struct timeval *from, struct timeval *sub) {
	long msec;
	msec =  (from->tv_sec - sub->tv_sec)*1000;
	msec += (from->tv_usec - sub->tv_usec)/1000;
	return msec;
}

static void tv_add_msecs(struct timeval *tv, long msecs) {
	tv->tv_sec += msecs / 1000;
	tv->tv_usec += (msecs % 1000) * 1000;
	if(tv->tv_usec >= 1000000) {
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}
}

static int b_read(struct dispatcher *dp, int ch, int size) {
	char hdr[16] = "R ";
	strncat(&hdr[2], ch_strs[ch], 10);
	struct buffer *b = &dp->ch[ch].rbuf;

	if(size == -1)
		size = buf_size_avail(b);

	int r = read(dp->ch[ch].fd, b->data + b->size, size);

        if(r > 0) {
                dbg1_h(hdr, b->data + b->size, r);
                buf_inc_size(b, r);
        }
        return r;
}

static int b_write(int fd, struct buffer *b, int ch) {
        int r = write(fd, b->data + b->rpos, b->size - b->rpos);
        if(r > 0) {
		char hdr[16] = "W ";
                strncat(&hdr[2], ch_strs[ch], 10);
                dbg1_h(hdr, b->data + b->rpos, r);
                buf_inc_rpos(b, r);
        }
        return r;
}

static const char *keyer_modes[] = {
	"CW", "VOICE", "FSK", "DIGITAL",
};

static int process_cmd(struct dispatcher *dp,  unsigned char *buf, int len) {
        struct cmd_request *req;
	unsigned char c;

	dbg0_h("R CTLCMD  ", buf, len);

	/* Check for cmds that may come in unsolicited. */
	switch(buf[0]) {
	case CMD_KEYER_MODE:
		if(len > 1) {
			c = buf[1];
			info("DSP Keyer Mode, cur: %s, r1: %s, r2: %s",
			     keyer_modes[c & 3], keyer_modes[(c>>2) & 3],
			     keyer_modes[(c>>4) & 3]);
		}
		break;

	case CMD_MPK_STATE:
		if(len >= 6)
			dbg0("DSP pwr: %2.1fV", ((float)buf[2])/10);
		break;

	case CMD_USB_RX_OVERFLOW:
		warn("DSP Device reports USB buffer overflow!");
		break;
	}

        req = (void*)PG_FIRSTENTRY(&dp->requests);

	if(!req || req->state != CRS_READ)
		return 0;
	if(buf[0] != req->b_cmd.data[0])
		return 0;

	dp_cancel_timer(dp, req->timer);
	dp_destroy_timer(req->timer);
	req->timer = NULL;
	PG_Remove(&req->node);
	if(buf_append(&req->b_resp, buf, len) < len) {
		warn("Buffer too small for command response!\n");
	}
	req->state = CRS_SUCCESS;
	if(req->req_done)
		req->req_done(req);
	return 0;
}

static void set_poll_events(struct dispatcher *dp) {
	struct cmd_request *req;
	unsigned i;

	dp->fds[KEYER_CHANNEL].events = POLLIN;

	for(i = 0; i < ARRAY_SIZE(dp->fds); i++) {
		dp->fds[i].fd = dp->ch[i].fd;

		if(dp->ch[KEYER_CHANNEL].fd == -1) {
			dp->fds[i].events = 0;
			continue;
		}

		if(i != KEYER_CHANNEL) {
			dp->fds[i].events = 0;

			switch(i) {
			case MH_CHANNEL_R1_FSK:
				if(dp->r1_fsk_busy)
					dbg1("Blocking while R1 FSK busy");
				else
					dp->fds[i].events = POLLIN;
				break;
			case MH_CHANNEL_R2_FSK:
				if(dp->r2_fsk_busy)
					dbg1("Blocking while R2 FSK busy");
				else
					dp->fds[i].events = POLLIN;
				break;
			default:
				dp->fds[i].events = POLLIN;
				break;
			}
		}

		if(i < KEYER_CHANNEL && dp->ch[i].rbuf.rpos < dp->ch[i].rbuf.size)
			dp->fds[KEYER_CHANNEL].events |= POLLOUT;

		if(dp->ch[i].fd != -1 && dp->ch[i].wbuf.rpos < dp->ch[i].wbuf.size)
			dp->fds[i].events |= POLLOUT;
	}

	req = (void*)PG_FIRSTENTRY(&dp->requests);
	if(req && (req->state == CRS_NEW || req->state == CRS_WRITE)) {
		dp->fds[KEYER_CHANNEL].events |= POLLOUT;
	}
}

static int process_ctlmsg(struct dispatcher *dp, int channel) {
	struct ctlmsg msg;
	unsigned char newflag;
	int ptt;

	int err = recv_ctlmsg(dp->ch[channel].fd, &msg);
	if(err)
		return -1;

	newflag = dp->keyer_flag;
	switch(msg.id) {
	case CMID_PTT:
		switch(channel) {
		case CTL_CHANNEL_CAT1:
		case CTL_CHANNEL_PTT1:
			ptt = MHC2DFL_PTT_R1;
			break;

		case CTL_CHANNEL_CAT2:
		case CTL_CHANNEL_PTT2:
			ptt = MHC2DFL_PTT_R2;
			break;
		default:
			return 0;
		}
		if(msg.data & 1)
			newflag |= ptt;
		else
			newflag &= ~ptt;

		dbg1("msg data %d flag: %d newflag %d", msg.data, dp->keyer_flag, newflag);

		if(newflag != dp->keyer_flag) {
			dbg1("*** flag change");
			dp->keyer_flag = newflag;
			info("newflag %d", newflag);
			buf_append_c(&dp->ch[MH_CHANNEL_FLAGS].rbuf, newflag);
		}

		break;
	}
	return 0;
}


static int ch_read_keyer(struct dispatcher *dp, int channel) {
	int r, c, kch, fl;

	r = b_read(dp, channel, -1);

	if(r < 0 && errno != EAGAIN) {
		err_e(-errno, "Error reading from channel %d!", channel);
		return -1;
	}
	if(r == 0)
		return 0;

	while((c = buf_get_c(&dp->ch[KEYER_CHANNEL].rbuf)) != -1) {
		kch = dmx_demux(dp->dmx, c);

		if(kch < 0 || kch >= MH_CHANNEL_MAX)
			continue;

		switch(kch) {
		case MH_CHANNEL_CONTROL:
			process_cmd(dp, dp->dmx->cmd_buffer, dp->dmx->cmd_length);
			break;
		case MH_CHANNEL_FLAGS:
			fl = dp->dmx->result_byte;
			dbg1("DEMUX CH FLAGS: %02x", dp->dmx->result_byte);

			if(!(fl & MHD2CFL_R2))
				dp->r1_fsk_busy = fl & MHD2CFL_FSK_BUSY;
			if(fl & MHD2CFL_R2)
				dp->r2_fsk_busy = fl & MHD2CFL_FSK_BUSY;
			break;
		default:
			dbg1("DEMUX CH %d: %02x", kch, dp->dmx->result_byte);

			if(dp->ch[kch].fd == -1)
				continue;
			if(buf_append_c(&dp->ch[kch].wbuf, dp->dmx->result_byte) < 0)
				err("Could not write to channel buffer %d", kch);
			break;
		}
	}

	return r;
}

static int ch_read_fsk(struct dispatcher *dp, int channel) {
	int r;

	if(channel == MH_CHANNEL_R1_FSK && dp->r1_fsk_busy)
		return 0;
	if(channel == MH_CHANNEL_R2_FSK && dp->r2_fsk_busy)
		return 0;

	r = b_read(dp, channel, 1);

	if(r < 0 && errno != EAGAIN) {
		err_e(-errno, "Error reading from channel %d!", channel);
		return -1;
	}
	if(r == 0)
		return 0;

	if(channel == MH_CHANNEL_R1_FSK)
		dp->r1_fsk_busy = 1;

	if(channel == MH_CHANNEL_R2_FSK)
		dp->r2_fsk_busy = 1;

	return r;
}

static int ch_read(struct dispatcher *dp, int channel) {
	int r;

	if(channel > KEYER_CHANNEL)
		return process_ctlmsg(dp, channel);

	r = b_read(dp, channel, -1);

	if(r < 0 && errno != EAGAIN) {
		err_e(-errno, "Error reading from channel %d!", channel);
		return -1;
	}
	if(r == 0)
		return 0;
		
	return r;
}

static int ch_write(struct dispatcher *dp, int channel) {
        int r;

	if(channel == KEYER_CHANNEL) {
		struct cmd_request *req = (void*)PG_FIRSTENTRY(&dp->requests);

		if(req && (req->state == CRS_NEW || req->state == CRS_WRITE)) {
			dp->mx->ibuf[MH_CHANNEL_CONTROL] = &req->b_cmd;
			req->state = CRS_WRITE;
		} else {
			req = NULL;
			dp->mx->ibuf[MH_CHANNEL_CONTROL] = NULL;
		}

                mx_mux(dp->mx, &dp->ch[KEYER_CHANNEL].wbuf);
                r = b_write(dp->ch[KEYER_CHANNEL].fd, &dp->ch[KEYER_CHANNEL].wbuf, channel);

		if(req) {
			if(req->b_cmd.rpos == req->b_cmd.size)
				req->state = CRS_READ;
		}

		if(r < 0 && errno != EAGAIN) {
			err_e(-errno, "Error writing to keyer!");
			return -1;
		}

	} else {

                r = b_write(dp->ch[channel].fd, &dp->ch[channel].wbuf, channel);

		if(r < 0 && errno != EAGAIN) {
			err_e(-errno, "Error writing to channel %d!", channel);
                        return -1;
                }

	}

	return 0;
}

/* Check for the next timeout.
 * return -1 if no timer in queue.
 * return 0 if already timed out.
 * otherwise return time in milliseconds when the next timer will timeout.
 */ 
static long check_timeout(struct dispatcher *dp) {
	struct timer *t;
	struct timeval tv;
	long expiry;
	expiry = -1;
	t = (void*)PG_FIRSTENTRY(&dp->timers);
	if(t) {
		gettimeofday(&tv, NULL);

		if(timercmp(&tv, &t->tv, >=))
			expiry = 0;
		else
			expiry = time_sub(&t->tv, &tv);
		if(expiry < 0)
			expiry = 0;
	}
	return expiry;
}

int dp_loop(struct dispatcher *dp, int return_on_no_req) {
	int r;
	long expiry;

	dbg1("DSP loop enter");

        dp->terminate = 0;
	sig_hup = 0;

	while(!dp->terminate && !(return_on_no_req && PG_LISTEMPTY(&dp->requests))) {
		if(sig_term) {
			dp_terminate(dp);
			info("Recevied signal %d, terminating", sig_term);
			continue;
		}

		set_poll_events(dp);
		expiry = check_timeout(dp);
		r = poll(dp->fds, ARRAY_SIZE(dp->fds), expiry);

		if(r == -1 && errno != EINTR) {
			err_e(-errno, "DSP poll error");
			return -1;
		}

		if(sig_hup && hup_handler && !return_on_no_req) {
			info("DSP SIGHUP received!");
			hup_handler(hup_handler_data);
			sig_hup = 0;
		}


		if(r > 0) {
			unsigned i;
			for(i = 0; i < ARRAY_SIZE(dp->fds); i++) {
				if(dp->fds[i].revents & POLLIN)
					dp->ch[i].read_func(dp, i);
				if(dp->fds[i].revents & POLLOUT)
					ch_write(dp, i);

                                if(dp->fds[i].revents & POLLHUP) {
                                        if(i == KEYER_CHANNEL) {
						err("Connection lost to keyer!");
                                                dp_terminate(dp);
                                        }
                                }
			}
		}

		expiry = check_timeout(dp);
		if(!expiry) {
			struct timer *t = (void*)PG_FIRSTENTRY(&dp->timers);
			int single = t->single_shot;
			PG_Remove(&t->node);
			if(t->timeout)
				t->timeout(t, t->userdata);
			if(!single)
				dp_submit_timer(dp, t);
		}

	}

	dbg1("DSP loop exit");

	return 0;
}

struct timer *dp_create_timer(struct dispatcher *dp, int ival, int single_shot,
				void (*timeout)(struct timer *, void *userdata),
				void *userdata) {
	struct timer *t = w_calloc(1, sizeof(*t));
	t->dp = dp;
	t->ival = ival;
	t->single_shot = single_shot;
	t->timeout = timeout;
	t->userdata = userdata;
	return t;
}

void dp_destroy_timer(struct timer *t) {
	struct timer *ts;
	PG_SCANLIST(&t->dp->timers, ts) {
		if(ts == t) {
			PG_Remove(&ts->node);
			break;
		}
	}
	free(t);
}

void dp_submit_timer(struct dispatcher *dp, struct timer *t) {
	struct timer *ts, *after;
	gettimeofday(&t->tv, NULL);
	tv_add_msecs(&t->tv, t->ival);

	ts = NULL;
	after = NULL;
	PG_SCANLIST(&dp->timers, ts) {
		if(timercmp(&ts->tv, &t->tv, >)) {
			after = (void*)ts->node.pn_Pred;
			break;
		}
	}
	if(after)
		PG_Insert(&dp->timers, &t->node, &after->node);
	else
		PG_AddTail(&dp->timers, &t->node);
}

void dp_cancel_timer(struct dispatcher *dp, struct timer *t) {
	struct timer *ts;
	int removed = 0;
	PG_SCANLIST(&dp->timers, ts) {
		if(t == ts) {
			PG_Remove(&ts->node);
			removed = 1;
			break;
		}
	}
	if(!removed)
		warn("DSP tried to cancel a non running timer");
}

void dp_terminate(struct dispatcher *dp) {
	struct cmd_request *req;

	while(!PG_LISTEMPTY(&dp->requests)) {
		req = (void*)PG_FIRSTENTRY(&dp->requests);
		PG_Remove(&req->node);
		req->state = CRS_ERROR;
		if(req->timer) {
			dp_destroy_timer(req->timer);
			req->timer = NULL;
		}
		req->req_done(req);
	}

	buf_append_c(&dp->ch[MH_CHANNEL_FLAGS].rbuf, 0);
	ch_write(dp, KEYER_CHANNEL);
	dp->terminate = 1;
}

void dp_set_keyer_fd(struct dispatcher *dp, int fd) {
	dp_set_fd(dp, KEYER_CHANNEL, fd);
}

void dp_set_fd(struct dispatcher *dp, int channel, int fd) {
	dp->ch[channel].fd = fd;
}

void dp_set_ctl_fd(struct dispatcher *dp, int channel, int fd) {
	dp->ch[channel + CTL_CHANNEL_OFF].fd = fd;
}

void dp_set_ptt_fd(struct dispatcher *dp, int radio, int fd) {
	switch(radio) {
	case 1:
		dp->ch[CTL_CHANNEL_PTT1].fd = fd;
		break;
	case 2:
		dp->ch[CTL_CHANNEL_PTT2].fd = fd;
		break;
	}
}

void dp_set_dev_timeout(struct dispatcher *dp, int timeout) {
	dp->dev_timeout = timeout;
}

void dp_set_hup_handler(void (*handler)(void *), void *userdata) {
	hup_handler = handler;
	hup_handler_data = userdata;
}
