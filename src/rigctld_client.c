/*
 *  mhuxd - microHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2024  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * rigctld_client.c
 *
 * TCP client that connects to a running rigctld instance, periodically
 * queries the rig's VFO frequency and operating mode, then forwards
 * the information to the microHam keyer via the vendor CAT frequency/mode
 * info commands (MHCMD_CAT_R1_FR_MODE_INFO / MHCMD_CAT_R2_FR_MODE_INFO).
 *
 * rigctld short-command protocol used here:
 *   Send "f\n"  →  receive "<freq_hz>\n"
 *   Send "m\n"  →  receive "<mode_str>\n<passband_hz>\n"
 *
 * keyer mode mapping (microHam internal):
 *   0 = CW      (CW, CWR)
 *   1 = VOICE   (USB, LSB, AM, FM, WFM, SAM, …)
 *   2 = FSK     (RTTY, RTTYR, PKTFM, PKTFMR)
 *   3 = DIGITAL (PKTUSB, PKTLSB, D-USB, D-LSB, …)
 */

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <ev.h>

#include "rigctld_client.h"
#include "mhcontrol.h"
#include "logger.h"
#include "util.h"

#define MOD_ID "rgc"

#define POLL_INTERVAL      2.0   /* seconds between frequency/mode polls */
#define RECONNECT_INTERVAL 5.0   /* seconds before reconnect attempt */
#define RGC_BUF_SIZE       128

/* keyer mode codes (match keyer_modes[] in mhcontrol.c) */
enum {
	KEYER_MODE_CW      = 0,
	KEYER_MODE_VOICE   = 1,
	KEYER_MODE_FSK     = 2,
	KEYER_MODE_DIGITAL = 3,
};

enum {
	RGC_STATE_DISCONNECTED,
	RGC_STATE_CONNECTING,
	RGC_STATE_IDLE,
	RGC_STATE_QUERY_FREQ,
	RGC_STATE_QUERY_MODE,
};

struct rigctld_client {
	struct ev_loop    *loop;
	struct mh_control *ctl;
	int                rig_idx;   /* 0 = R1, 1 = R2 */
	char              *host;
	int                port;

	int                fd;
	int                state;

	ev_io              w_io;
	ev_timer           reconnect_timer;
	ev_timer           poll_timer;

	char               buf[RGC_BUF_SIZE];
	int                buf_len;

	uint32_t           freq_hz;
	uint8_t            mode;
};

/* ------------------------------------------------------------------ */
/* helpers                                                              */
/* ------------------------------------------------------------------ */

static uint8_t parse_mode(const char *s)
{
	if (!strncmp(s, "CW",    2)) return KEYER_MODE_CW;
	if (!strncmp(s, "USB",   3) ||
	    !strncmp(s, "LSB",   3) ||
	    !strncmp(s, "AM",    2) ||
	    !strncmp(s, "FM",    2) ||
	    !strncmp(s, "WFM",   3) ||
	    !strncmp(s, "SAM",   3) ||
	    !strncmp(s, "SAL",   3) ||
	    !strncmp(s, "SAH",   3)) return KEYER_MODE_VOICE;
	if (!strncmp(s, "RTTY",  4) ||
	    !strncmp(s, "PKTFM", 5)) return KEYER_MODE_FSK;
	if (!strncmp(s, "PKT",   3) ||
	    !strncmp(s, "D-USB", 5) ||
	    !strncmp(s, "D-LSB", 5) ||
	    !strncmp(s, "DIG",   3)) return KEYER_MODE_DIGITAL;
	return KEYER_MODE_CW;
}

static void update_keyer(struct rigctld_client *rgc)
{
	dbg1("%s rig%d freq=%u mode=%u", rgc->host, rgc->rig_idx + 1,
	     rgc->freq_hz, rgc->mode);

	if (rgc->rig_idx == 0)
		mhc_cat_set_r1_freq_mode(rgc->ctl, rgc->freq_hz, rgc->mode);
	else
		mhc_cat_set_r2_freq_mode(rgc->ctl, rgc->freq_hz, rgc->mode);
}

/* ------------------------------------------------------------------ */
/* connection management                                                */
/* ------------------------------------------------------------------ */

static void disconnect(struct rigctld_client *rgc);
static void reconnect_cb(struct ev_loop *loop, ev_timer *w, int revents);

static void schedule_reconnect(struct rigctld_client *rgc)
{
	disconnect(rgc);
	ev_timer_set(&rgc->reconnect_timer, RECONNECT_INTERVAL, 0.);
	ev_timer_start(rgc->loop, &rgc->reconnect_timer);
}

static void disconnect(struct rigctld_client *rgc)
{
	ev_io_stop(rgc->loop, &rgc->w_io);
	ev_timer_stop(rgc->loop, &rgc->poll_timer);
	if (rgc->fd != -1) {
		close(rgc->fd);
		rgc->fd = -1;
	}
	rgc->buf_len = 0;
	rgc->state   = RGC_STATE_DISCONNECTED;
}

static int try_connect(struct rigctld_client *rgc)
{
	char port_str[16];
	struct addrinfo hints, *res;
	int fd, r;

	snprintf(port_str, sizeof(port_str), "%d", rgc->port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	r = getaddrinfo(rgc->host, port_str, &hints, &res);
	if (r != 0) {
		warn("%s:%d getaddrinfo: %s", rgc->host, rgc->port,
		     gai_strerror(r));
		return -1;
	}

	fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (fd == -1) {
		warn_e(errno, "%s:%d socket()", rgc->host, rgc->port);
		freeaddrinfo(res);
		return -1;
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		warn_e(errno, "%s:%d fcntl(O_NONBLOCK)", rgc->host, rgc->port);
		close(fd);
		freeaddrinfo(res);
		return -1;
	}

	r = connect(fd, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	if (r == -1 && errno != EINPROGRESS) {
		dbg1("%s:%d connect() failed: %s", rgc->host, rgc->port,
		     strerror(errno));
		close(fd);
		return -1;
	}

	rgc->fd    = fd;
	rgc->state = RGC_STATE_CONNECTING;
	return 0;
}

/* ------------------------------------------------------------------ */
/* I/O                                                                  */
/* ------------------------------------------------------------------ */

static void send_query(struct rigctld_client *rgc, const char *cmd)
{
	ssize_t r = write(rgc->fd, cmd, strlen(cmd));
	if (r <= 0)
		dbg1("%s:%d write failed", rgc->host, rgc->port);
}

static void poll_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
	(void)loop; (void)revents;
	struct rigctld_client *rgc = w->data;

	rgc->buf_len = 0;
	rgc->state   = RGC_STATE_QUERY_FREQ;
	send_query(rgc, "f\n");
	ev_io_set(&rgc->w_io, rgc->fd, EV_READ);
	ev_io_start(rgc->loop, &rgc->w_io);
}

static void handle_read(struct rigctld_client *rgc)
{
	char *nl;
	ssize_t r;
	int avail = (int)sizeof(rgc->buf) - rgc->buf_len - 1;

	if (avail <= 0) {
		warn("%s:%d receive buffer full, resetting", rgc->host,
		     rgc->port);
		rgc->buf_len = 0;
		return;
	}

	r = read(rgc->fd, rgc->buf + rgc->buf_len, (size_t)avail);
	if (r <= 0) {
		if (r == 0 || (errno != EAGAIN && errno != EINTR)) {
			info("%s:%d connection closed", rgc->host, rgc->port);
			schedule_reconnect(rgc);
		}
		return;
	}
	rgc->buf_len += (int)r;
	rgc->buf[rgc->buf_len] = '\0';

	/* process complete lines */
	while ((nl = memchr(rgc->buf, '\n', (size_t)rgc->buf_len)) != NULL) {
		*nl = '\0';
		int line_len = (int)(nl - rgc->buf) + 1;

		switch (rgc->state) {
		case RGC_STATE_QUERY_FREQ: {
			char *end;
			long long v = strtoll(rgc->buf, &end, 10);
			if (end != rgc->buf && v > 0) {
				rgc->freq_hz = (uint32_t)v;
				/* now query mode */
				rgc->state = RGC_STATE_QUERY_MODE;
				memmove(rgc->buf, nl + 1,
					(size_t)(rgc->buf_len - line_len));
				rgc->buf_len -= line_len;
				rgc->buf[rgc->buf_len] = '\0';
				send_query(rgc, "m\n");
				continue; /* re-check buffer for mode reply */
			} else {
				dbg1("%s:%d invalid frequency reply: '%s'",
				     rgc->host, rgc->port, rgc->buf);
				schedule_reconnect(rgc);
				return;
			}
		}
		case RGC_STATE_QUERY_MODE:
			rgc->mode = parse_mode(rgc->buf);
			update_keyer(rgc);
			/* switch to idle, wait for next poll */
			ev_io_stop(rgc->loop, &rgc->w_io);
			rgc->state   = RGC_STATE_IDLE;
			rgc->buf_len = 0;
			ev_timer_set(&rgc->poll_timer, POLL_INTERVAL, 0.);
			ev_timer_start(rgc->loop, &rgc->poll_timer);
			return;
		default:
			break;
		}

		memmove(rgc->buf, nl + 1, (size_t)(rgc->buf_len - line_len));
		rgc->buf_len -= line_len;
		rgc->buf[rgc->buf_len] = '\0';
	}
}

static void io_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	(void)loop;
	struct rigctld_client *rgc = w->data;

	if (rgc->state == RGC_STATE_CONNECTING) {
		/* check whether non-blocking connect succeeded */
		int err = 0;
		socklen_t len = sizeof(err);
		getsockopt(rgc->fd, SOL_SOCKET, SO_ERROR, &err, &len);
		if (err != 0) {
			dbg1("%s:%d connect failed: %s", rgc->host, rgc->port,
			     strerror(err));
			schedule_reconnect(rgc);
			return;
		}
		info("%s:%d rigctld connected", rgc->host, rgc->port);
		/* kick off first poll immediately */
		ev_io_stop(rgc->loop, &rgc->w_io);
		rgc->state = RGC_STATE_IDLE;
		poll_cb(rgc->loop, &rgc->poll_timer, 0);
		return;
	}

	if (revents & EV_READ)
		handle_read(rgc);

	if (revents & EV_ERROR) {
		err("%s:%d I/O error", rgc->host, rgc->port);
		schedule_reconnect(rgc);
	}
}

static void reconnect_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
	(void)loop; (void)revents;
	struct rigctld_client *rgc = w->data;

	dbg1("%s:%d reconnecting", rgc->host, rgc->port);
	if (try_connect(rgc) == 0) {
		/* wait for connect to complete (writable event) */
		ev_io_init(&rgc->w_io, io_cb, rgc->fd, EV_WRITE);
		rgc->w_io.data = rgc;
		ev_io_start(rgc->loop, &rgc->w_io);
	} else {
		/* retry later */
		ev_timer_set(&rgc->reconnect_timer, RECONNECT_INTERVAL, 0.);
		ev_timer_start(rgc->loop, &rgc->reconnect_timer);
	}
}

/* ------------------------------------------------------------------ */
/* public API                                                           */
/* ------------------------------------------------------------------ */

struct rigctld_client *rgc_create(struct ev_loop *loop,
				  struct mh_control *ctl,
				  const char *host, int port, int rig_idx)
{
	struct rigctld_client *rgc;

	rgc           = w_calloc(1, sizeof(*rgc));
	rgc->loop     = loop;
	rgc->ctl      = ctl;
	rgc->rig_idx  = rig_idx;
	rgc->host     = w_strdup(host);
	rgc->port     = port;
	rgc->fd       = -1;
	rgc->state    = RGC_STATE_DISCONNECTED;

	ev_timer_init(&rgc->reconnect_timer, reconnect_cb, 0., 0.);
	rgc->reconnect_timer.data = rgc;

	ev_timer_init(&rgc->poll_timer, poll_cb, 0., 0.);
	rgc->poll_timer.data = rgc;

	/* start connection attempt immediately */
	if (try_connect(rgc) == 0) {
		ev_io_init(&rgc->w_io, io_cb, rgc->fd, EV_WRITE);
		rgc->w_io.data = rgc;
		ev_io_start(rgc->loop, &rgc->w_io);
	} else {
		ev_timer_set(&rgc->reconnect_timer, RECONNECT_INTERVAL, 0.);
		ev_timer_start(rgc->loop, &rgc->reconnect_timer);
	}

	info("%s:%d rigctld client created (rig %d)", host, port, rig_idx + 1);
	return rgc;
}

void rgc_destroy(struct rigctld_client *rgc)
{
	if (!rgc)
		return;
	disconnect(rgc);
	ev_timer_stop(rgc->loop, &rgc->reconnect_timer);
	free(rgc->host);
	free(rgc);
}
