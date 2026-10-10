/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Native CAT own queries end to end, through the real router (src/mhrouter.c):
 * an emulated keyer with an IC-7300 on the CI-V bus behind a socketpair, and a
 * client application on a connector socketpair, wired as conmgr does it.
 *
 * Checks that mhuxd asks the radio only while the client doesn't poll, that
 * the client never sees a frame of mhuxd's queries, and that every client
 * command is answered in time. Built with the short own query intervals of
 * native_test (see Makefile).
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "cat_native.h"
#include "app_ctx.h"
#include "channel.h"
#include "device.h"
#include "mhrouter.h"
#include "stubs.h"

#define RADIO_ADDR 0x94
#define CLIENT_ADDR 0xe0
#define OWN_ADDR 0xe5
#define CLIENT_TIMEOUT 1.0	// Hamlib's timeout for Icom radios
#define RADIO_DELAY 0.02	// radio answer after the command

/*
 * Stubs for the router's dependencies
 */

const char *ch_channel2str(int channel) {
	return channel == MH_CHANNEL_R1 ? "R1" : "?";
}

void tty_close(int fd) {
	close(fd);
}

/*
 * Keyer and radio: the keyer side of the microHAM protocol (src/mux.c, src/demux.c) for
 * channel R1, and an IC-7300 answering CI-V commands with bus echo.
 */

static int kfd[2] = { -1, -1 };
static ev_io keyer_w;
static int k_sync = -1, k_idx;

static struct {
	uint32_t freq;
	uint8_t mode, data;	// CI-V mode code, data mode flag
	int transceive;
	int off;
} rig = { 14074000, 0x01, 1, 0, 0 };

static uint8_t civ_rx[64];
static size_t civ_rx_len;
static int own_cmds, client_cmds;	// commands the radio got from mhuxd and the client

// Radio output, sent when due.
#define OUTQ_MAX 32
static struct {
	ev_tstamp due;
	uint8_t buf[32];
	size_t len;
} outq[OUTQ_MAX];
static int noutq;
static ev_timer outq_timer;

// One byte per sequence, R1 only: sync byte with the MSB of the data, then the payload.
static void keyer_send(const uint8_t *buf, size_t len) {
	for(size_t i = 0; i < len; i++) {
		uint8_t f[4] = { 0x20 | ((buf[i] >> 7) << 2), 0x80 | (buf[i] & 0x7f), 0x80, 0x80 };
		if(write(kfd[1], f, sizeof(f)) != sizeof(f))
			CHECK(0, "keyer write: %s", strerror(errno));
	}
}

static void outq_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	ev_tstamp now = ev_now(l);
	(void)w; (void)revents;
	while(noutq && outq[0].due <= now) {
		keyer_send(outq[0].buf, outq[0].len);
		memmove(&outq[0], &outq[1], (noutq - 1) * sizeof(outq[0]));
		noutq--;
	}
}

static void radio_out(double delay, const uint8_t *buf, size_t len) {
	if(noutq == OUTQ_MAX || len > sizeof(outq[0].buf))
		return;
	outq[noutq].due = ev_now(loop) + delay;
	memcpy(outq[noutq].buf, buf, len);
	outq[noutq].len = len;
	noutq++;
}

static void radio_frame(uint8_t to, uint8_t cmd, const uint8_t *data, size_t n) {
	uint8_t f[32] = { 0xfe, 0xfe, to, RADIO_ADDR, cmd };
	memcpy(f + 5, data, n);
	f[5 + n] = 0xfd;
	radio_out(RADIO_DELAY, f, 6 + n);
}

static void bcd_freq(uint32_t hz, uint8_t *p) {
	for(int i = 0; i < 5; i++) {
		p[i] = (hz % 10) | ((hz / 10 % 10) << 4);
		hz /= 100;
	}
}

// A complete frame on the bus: echo, then the radio's answer if it is for the radio.
static void radio_cmd(const uint8_t *f, size_t len) {
	const uint8_t *d = f + 5;
	size_t n = len - 6;
	uint8_t ctl = f[3], r[16];

	radio_out(0.002, f, len);
	if(f[2] != RADIO_ADDR || rig.off)
		return;
	own_cmds += ctl == OWN_ADDR;
	client_cmds += ctl == CLIENT_ADDR;

	switch(f[4]) {
	case 0x03:
		bcd_freq(rig.freq, r);
		radio_frame(ctl, 0x03, r, 5);
		return;
	case 0x04:
		r[0] = rig.mode;
		r[1] = 0x01;
		radio_frame(ctl, 0x04, r, 2);
		return;
	case 0x26:
		if(n == 1) {
			r[0] = d[0];
			r[1] = rig.mode;
			r[2] = rig.data;
			r[3] = 0x01;
			radio_frame(ctl, 0x26, r, 4);
			return;
		}
		break;
	case 0x1a:
		if(n == 1 && d[0] == 0x06) {
			r[0] = 0x06;
			r[1] = rig.data;
			r[2] = 0x01;
			radio_frame(ctl, 0x1a, r, 3);
			return;
		}
		break;
	}
	radio_frame(ctl, 0xfa, NULL, 0);
}

static void radio_rx(uint8_t b) {
	if(b == 0xfe && civ_rx_len < 2) {
		civ_rx[civ_rx_len++] = b;
		return;
	}
	if(civ_rx_len < 2) {
		civ_rx_len = 0;
		return;
	}
	if(civ_rx_len < sizeof(civ_rx))
		civ_rx[civ_rx_len++] = b;
	if(b == 0xfd) {
		if(civ_rx_len >= 6)
			radio_cmd(civ_rx, civ_rx_len);
		civ_rx_len = 0;
	}
}

static void keyer_cb(struct ev_loop *l, struct ev_io *w, int revents) {
	uint8_t buf[256];
	ssize_t r = read(w->fd, buf, sizeof(buf));
	(void)l; (void)revents;

	for(ssize_t i = 0; i < r; i++) {
		uint8_t c = buf[i];
		if(!(c & 0x80)) {
			k_sync = c;
			k_idx = 0;
			continue;
		}
		k_idx++;
		if(k_sync >= 0 && k_idx == 1 && (k_sync & 0x20))
			radio_rx((c & 0x7f) | (((k_sync >> 2) & 1) << 7));
	}
}

// Change on the front panel; sent as transceive broadcast if enabled.
static void panel_mode(uint8_t mode, uint8_t data) {
	rig.mode = mode;
	rig.data = data;
	if(rig.transceive) {
		uint8_t r[2] = { mode, 0x01 };
		radio_frame(0x00, 0x01, r, 2);
	}
}

/*
 * Client application, polling like Hamlib: one command at a time, waiting for its reply.
 */

static int cfd[2] = { -1, -1 };
static ev_io client_w;
static ev_timer client_timer;

static struct {
	double ival;		// poll interval, 0 = back to back, < 0 = idle
	int step;		// command of the current poll
	int busy;
	ev_tstamp sent;
	uint8_t rx[64];
	size_t rx_len;
	// results
	int done, timeouts, foreign, unknown, transceive;
	double max_latency;
} cl;

static const uint8_t client_polls[][7] = {
	{ 0xfe, 0xfe, RADIO_ADDR, CLIENT_ADDR, 0x03, 0xfd },
	{ 0xfe, 0xfe, RADIO_ADDR, CLIENT_ADDR, 0x26, 0x00, 0xfd },
};
static const size_t client_poll_len[] = { 6, 7 };

static void client_next(void);

static void client_send(void) {
	const uint8_t *q = client_polls[cl.step];
	cl.busy = 1;
	cl.sent = ev_now(loop);
	if(write(cfd[1], q, client_poll_len[cl.step]) != (ssize_t)client_poll_len[cl.step])
		CHECK(0, "client write: %s", strerror(errno));
	ev_timer_stop(loop, &client_timer);
	ev_timer_set(&client_timer, CLIENT_TIMEOUT, 0.);
	ev_timer_start(loop, &client_timer);
}

static void client_done(void) {
	double lat = ev_now(loop) - cl.sent;
	if(lat > cl.max_latency)
		cl.max_latency = lat;
	cl.done++;
	cl.busy = 0;
	ev_timer_stop(loop, &client_timer);
	client_next();
}

// Next command of the poll, or the next poll after the interval.
static void client_next(void) {
	if(cl.ival < 0)
		return;
	if(++cl.step < (int)(sizeof(client_polls) / sizeof(client_polls[0]))) {
		client_send();
		return;
	}
	cl.step = -1;
	ev_timer_set(&client_timer, cl.ival, 0.);
	ev_timer_start(loop, &client_timer);
}

static void client_timer_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	(void)l; (void)w; (void)revents;
	if(cl.busy) {
		cl.timeouts++;
		cl.busy = 0;
	}
	client_next();
}

static void client_frame(const uint8_t *f, size_t len) {
	if(len < 6)
		return;
	if(f[2] == OWN_ADDR || f[3] == OWN_ADDR) {
		cl.foreign++;	// a frame of mhuxd's own queries reached the client
		return;
	}
	if(f[2] == RADIO_ADDR && f[3] == CLIENT_ADDR)
		return;	// echo
	if(f[2] == 0x00 && f[3] == RADIO_ADDR) {
		cl.transceive++;
		return;
	}
	if(f[2] == CLIENT_ADDR && f[3] == RADIO_ADDR && cl.busy && f[4] == client_polls[cl.step][4]) {
		client_done();
		return;
	}
	cl.unknown++;
}

static void client_cb(struct ev_loop *l, struct ev_io *w, int revents) {
	uint8_t buf[256];
	ssize_t r = read(w->fd, buf, sizeof(buf));
	(void)l; (void)revents;

	for(ssize_t i = 0; i < r; i++) {
		uint8_t b = buf[i];
		if(b == 0xfe && cl.rx_len < 2) {
			cl.rx[cl.rx_len++] = b;
			continue;
		}
		if(cl.rx_len < 2) {
			cl.unknown++;	// bytes outside a frame
			cl.rx_len = 0;
			continue;
		}
		if(cl.rx_len < sizeof(cl.rx))
			cl.rx[cl.rx_len++] = b;
		if(b == 0xfd) {
			client_frame(cl.rx, cl.rx_len);
			cl.rx_len = 0;
		}
	}
}

// Starts polling (ival >= 0) or stops it (ival < 0) once the command in progress is answered.
static void client_poll(double ival) {
	cl.ival = ival;
	if(cl.busy)
		return;
	ev_timer_stop(loop, &client_timer);
	cl.step = -1;
	if(ival >= 0)
		client_next();
}

static void client_reset(void) {
	cl.done = cl.timeouts = cl.foreign = cl.unknown = cl.transceive = 0;
	cl.max_latency = 0;
}

/*
 * Tests
 */

static int nonblock_pair(int fd[2]) {
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, fd))
		return -1;
	fcntl(fd[0], F_SETFL, O_NONBLOCK);
	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	return 0;
}

int main(void) {
	struct device dev = { .ctl = (struct mh_control *)&dev, .serial = "TEST0001" };
	struct cat_native_cfg cfg = { .radio = 1, .own_queries = 1 };
	const struct mhc_radio_info *p;
	int own;

	loop = ev_default_loop(0);
	if(nonblock_pair(kfd) || nonblock_pair(cfd)) {
		printf("FAIL router_test: socketpair\n");
		return 1;
	}

	dev.router = mhr_create(loop, "TEST0001", 0);
	mhr_set_keyer_fd(dev.router, kfd[0]);
	mhr_add_endpoint_fd(dev.router, cfd[0], MH_CHANNEL_R1, "test");
	ev_io_init(&keyer_w, keyer_cb, kfd[1], EV_READ);
	ev_io_start(loop, &keyer_w);
	ev_io_init(&client_w, client_cb, cfd[1], EV_READ);
	ev_io_start(loop, &client_w);
	ev_timer_init(&outq_timer, outq_cb, 0.002, 0.002);
	ev_timer_start(loop, &outq_timer);
	ev_timer_init(&client_timer, client_timer_cb, 0., 0.);

	speed.rigtype = RIG_IC7300;
	speed.icomaddress = RADIO_ADDR;
	speed_valid = 1;
	struct cat_native *cn = cat_native_create((app_ctx *)&dev, &dev, &cfg);

	// Client idle: mhuxd asks, the client gets nothing of it.
	cl.step = -1;
	client_poll(-1);
	run_for(1.0);
	p = last_pub();
	CHECK(p && p->rxFreq == 14074000 && p->mode == MOD_DIGITAL, "own queries: 14074000 PKTUSB");
	CHECK(own_cmds >= 2, "own queries sent: %d", own_cmds);
	CHECK(!cl.foreign && !cl.unknown, "client got nothing (%d own frames, %d other)", cl.foreign, cl.unknown);

	// Front panel change without transceive: found by the own queries.
	panel_mode(0x03, 0);
	run_for(1.2);
	p = last_pub();
	CHECK(p && p->mode == MOD_CW, "front panel change to CW found, keyer mode %d", p ? p->mode : -9);

	// Client polling faster than values go stale: no own queries, every command answered.
	client_poll(0.3);
	run_for(0.7);
	own = own_cmds;
	client_reset();
	run_for(2.0);
	CHECK(own_cmds == own, "no own queries while the client polls, got %d", own_cmds - own);
	CHECK(cl.done >= 10 && !cl.timeouts && !cl.foreign && !cl.unknown,
	      "client polling: %d answered, %d timeouts, %d own frames, %d other", cl.done, cl.timeouts,
	      cl.foreign, cl.unknown);

	// Client polling slower: own queries in the gaps, the client doesn't notice.
	client_poll(1.0);
	run_for(0.3);
	own = own_cmds;
	client_reset();
	panel_mode(0x01, 1);
	run_for(4.0);
	CHECK(own_cmds > own, "own queries between slow client polls: %d", own_cmds - own);
	CHECK(cl.done >= 6 && !cl.timeouts && !cl.foreign && !cl.unknown,
	      "slow client polling: %d answered, %d timeouts, %d own frames, %d other", cl.done, cl.timeouts,
	      cl.foreign, cl.unknown);
	CHECK(cl.max_latency < 0.2, "slow client polling: max latency %.3f s", cl.max_latency);

	// Client back to back: no gap for own queries.
	client_poll(0);
	run_for(0.5);
	own = own_cmds;
	client_reset();
	run_for(1.5);
	CHECK(own_cmds == own, "no own queries between back to back client commands, got %d", own_cmds - own);
	CHECK(cl.done >= 20 && !cl.timeouts && !cl.foreign && !cl.unknown,
	      "back to back: %d answered, %d timeouts, %d own frames, %d other", cl.done, cl.timeouts,
	      cl.foreign, cl.unknown);

	// Transceive broadcasts during own queries reach the client.
	client_poll(-1);
	rig.transceive = 1;
	run_for(0.7);
	client_reset();
	for(int i = 0; i < 10; i++) {
		panel_mode(i & 1 ? 0x03 : 0x01, 0);
		run_for(0.13);
	}
	run_for(0.3);
	CHECK(cl.transceive == 10 && !cl.foreign && !cl.unknown,
	      "transceive: %d of 10 broadcasts, %d own frames, %d other at the client", cl.transceive,
	      cl.foreign, cl.unknown);

	cat_native_destroy(cn);
	mhr_destroy(dev.router);
	close(kfd[1]);
	close(cfd[1]);
	printf("%s router_test: radio got %d own queries, %d client commands\n", failures ? "FAIL" : "ok  ",
	       own_cmds, client_cmds);
	return failures ? 1 : 0;
}
