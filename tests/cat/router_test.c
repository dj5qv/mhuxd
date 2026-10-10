/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Native CAT end to end, through the real router (src/mhrouter.c): an emulated
 * keyer with an IC-7300 on the CI-V bus behind a socketpair, and client
 * applications on connector socketpairs, wired as conmgr does it.
 *
 * Own queries: mhuxd asks the radio only while the client doesn't poll, the
 * client never sees a frame of mhuxd's queries, and every client command is
 * answered in time. Two clients on two connectors: one command at a time on
 * the bus (the radio counts collisions), each client gets the echo and answers
 * of its own commands only, transceive goes to both, a client whose command
 * isn't answered doesn't block the other for long. Built with the short own
 * query intervals of native_test (see Makefile).
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
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
static struct keyer_rx krx = { -1, 0 };

#define SILENT_CMD 0x30	// the radio doesn't answer this command

static struct {
	uint32_t freq;
	uint8_t mode, data;	// CI-V mode code, data mode flag
	int transceive;
	int off;
} rig = { 14074000, 0x01, 1, 0, 0 };

static uint8_t civ_rx[64];
static size_t civ_rx_len;
static int own_cmds, client_cmds;	// commands the radio got from mhuxd and the clients
static int collisions;			// commands while the radio's answer was still due
static ev_tstamp answer_due;
static int jams;			// commands destroyed by a transceive broadcast (radio sent FC)
static ev_timer tune_timer;		// VFO knob turned: a transceive broadcast every 30 ms

// Radio output, sent when due.
#define OUTQ_MAX 32
static struct {
	ev_tstamp due;
	uint8_t buf[32];
	size_t len;
} outq[OUTQ_MAX];
static int noutq;
static ev_timer outq_timer;

static void keyer_send(const uint8_t *buf, size_t len) {
	for(size_t i = 0; i < len; i++) {
		uint8_t f[4];
		keyer_r1_encode(buf[i], f);
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
	if(to != 0x00)
		answer_due = ev_now(loop) + RADIO_DELAY;
}

// A broadcast reaches the host when due; it was on the bus from 10 to 4 ms before. A command
// that arrives meanwhile collides with it: the broadcast is cut off, the radio sends FC five
// times and drops the command (as an IC-7300 behind a DK2 does).
static int radio_collision(void) {
	ev_tstamp now = ev_now(loop);
	for(int i = 0; i < noutq; i++) {
		if(outq[i].buf[2] != 0x00 || outq[i].due <= now || outq[i].due > now + 0.010)
			continue;
		if(noutq == OUTQ_MAX)
			return 0;
		outq[i].len = 9;
		memmove(&outq[i + 2], &outq[i + 1], (noutq - i - 1) * sizeof(outq[0]));
		noutq++;
		outq[i + 1].due = outq[i].due;
		memset(outq[i + 1].buf, 0xfc, 5);
		outq[i + 1].len = 5;
		jams++;
		return 1;
	}
	return 0;
}

static void bcd_freq(uint32_t hz, uint8_t *p);

static void tune_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	uint8_t r[5];
	(void)l; (void)w; (void)revents;
	rig.freq += 10;
	bcd_freq(rig.freq, r);
	radio_frame(0x00, 0x00, r, 5);
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

	// On the single-wire bus a command sent while the radio answers collides.
	if(ev_now(loop) < answer_due)
		collisions++;
	if(radio_collision())
		return;
	radio_out(0.002, f, len);
	if(f[2] != RADIO_ADDR || rig.off)
		return;
	own_cmds += ctl == OWN_ADDR;
	client_cmds += ctl == CLIENT_ADDR;
	if(f[4] == SILENT_CMD)
		return;

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
	case 0x15:	// meters: 02 S-meter, 11 RF power
		if(n == 1 && (d[0] == 0x02 || d[0] == 0x11)) {
			r[0] = d[0];
			r[1] = 0x01;
			r[2] = 0x20;
			radio_frame(ctl, 0x15, r, 3);
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
		uint8_t b;
		if(keyer_r1_decode(&krx, buf[i], &b))
			radio_rx(b);
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
 * Client applications, polling like Hamlib: one command at a time, waiting for its reply.
 * Each one asks different commands, so a frame that reaches the wrong client is noticed.
 */

#define NCLIENTS 2
#define MAX_POLLS 3

struct client {
	int fd[2];
	ev_io w;
	ev_timer timer;
	uint8_t polls[MAX_POLLS][8];
	size_t poll_len[MAX_POLLS];
	int npolls;
	double ival;		// poll interval, 0 = back to back, < 0 = idle
	int step;		// command of the current poll
	int busy, echoed;
	ev_tstamp sent;
	uint8_t rx[64];
	size_t rx_len;
	// results
	int done, timeouts, foreign, misrouted, unknown, transceive;
	double max_latency;
};

static struct client cl[NCLIENTS];

static void client_add_poll(struct client *c, const uint8_t *cmd, size_t n) {
	uint8_t *q = c->polls[c->npolls];
	q[0] = q[1] = 0xfe;
	q[2] = RADIO_ADDR;
	q[3] = CLIENT_ADDR;
	memcpy(q + 4, cmd, n);
	q[4 + n] = 0xfd;
	c->poll_len[c->npolls++] = 5 + n;
}

static void client_set_poll(struct client *c, const uint8_t *cmd, size_t n) {
	c->npolls = 0;
	client_add_poll(c, cmd, n);
}

static void client_next(struct client *c);

static void client_send(struct client *c) {
	const uint8_t *q = c->polls[c->step];
	c->busy = 1;
	c->echoed = 0;
	c->sent = ev_now(loop);
	if(write(c->fd[1], q, c->poll_len[c->step]) != (ssize_t)c->poll_len[c->step])
		CHECK(0, "client write: %s", strerror(errno));
	ev_timer_stop(loop, &c->timer);
	ev_timer_set(&c->timer, CLIENT_TIMEOUT, 0.);
	ev_timer_start(loop, &c->timer);
}

static void client_done(struct client *c) {
	double lat = ev_now(loop) - c->sent;
	if(lat > c->max_latency)
		c->max_latency = lat;
	c->done++;
	c->busy = 0;
	ev_timer_stop(loop, &c->timer);
	client_next(c);
}

// Next command of the poll, or the next poll after the interval.
static void client_next(struct client *c) {
	if(c->ival < 0)
		return;
	if(++c->step < c->npolls) {
		client_send(c);
		return;
	}
	c->step = -1;
	ev_timer_set(&c->timer, c->ival, 0.);
	ev_timer_start(loop, &c->timer);
}

static void client_timer_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	struct client *c = w->data;
	(void)l; (void)revents;
	if(c->busy) {
		c->timeouts++;
		c->busy = 0;
	}
	client_next(c);
}

static void client_frame(struct client *c, const uint8_t *f, size_t len) {
	const uint8_t *q = c->busy ? c->polls[c->step] : NULL;

	if(len < 6)
		return;
	if(f[2] == OWN_ADDR || f[3] == OWN_ADDR) {
		c->foreign++;	// a frame of mhuxd's own queries reached the client
		return;
	}
	if(f[2] == 0x00 && f[3] == RADIO_ADDR) {
		c->transceive++;
		return;
	}
	if(f[2] == RADIO_ADDR && f[3] == CLIENT_ADDR) {
		// Echo: of the command in progress, once.
		if(q && !c->echoed && len == c->poll_len[c->step] && !memcmp(f, q, len))
			c->echoed = 1;
		else
			c->misrouted++;
		return;
	}
	if(f[2] == CLIENT_ADDR && f[3] == RADIO_ADDR) {
		// Reply: same command and sub command (all polls with data have one), or FA.
		if(q && ((f[4] == q[4] && (c->poll_len[c->step] <= 6 || f[5] == q[5])) || f[4] == 0xfa))
			client_done(c);
		else
			c->misrouted++;	// another client's answer
		return;
	}
	c->unknown++;
}

static void client_cb(struct ev_loop *l, struct ev_io *w, int revents) {
	struct client *c = w->data;
	uint8_t buf[256];
	ssize_t r = read(w->fd, buf, sizeof(buf));
	(void)l; (void)revents;

	for(ssize_t i = 0; i < r; i++) {
		uint8_t b = buf[i];
		if(b == 0xfe && c->rx_len < 2) {
			c->rx[c->rx_len++] = b;
			continue;
		}
		if(c->rx_len < 2) {
			c->unknown++;	// bytes outside a frame
			c->rx_len = 0;
			continue;
		}
		if(c->rx_len < sizeof(c->rx))
			c->rx[c->rx_len++] = b;
		if(b == 0xfd) {
			client_frame(c, c->rx, c->rx_len);
			c->rx_len = 0;
		}
	}
}

// Starts polling (ival >= 0) or stops it (ival < 0) once the command in progress is answered.
static void client_poll(struct client *c, double ival) {
	c->ival = ival;
	if(c->busy)
		return;
	ev_timer_stop(loop, &c->timer);
	c->step = -1;
	if(ival >= 0)
		client_next(c);
}

static void client_reset(struct client *c) {
	c->done = c->timeouts = c->foreign = c->misrouted = c->unknown = c->transceive = 0;
	c->max_latency = 0;
}

static int nonblock_pair(int fd[2]) {
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, fd))
		return -1;
	fcntl(fd[0], F_SETFL, O_NONBLOCK);
	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	return 0;
}

static int client_init(struct client *c) {
	if(nonblock_pair(c->fd))
		return -1;
	c->step = -1;
	c->ival = -1;
	ev_io_init(&c->w, client_cb, c->fd[1], EV_READ);
	c->w.data = c;
	ev_io_start(loop, &c->w);
	ev_timer_init(&c->timer, client_timer_cb, 0., 0.);
	c->timer.data = c;
	return 0;
}

// Nothing reached the client that wasn't its own.
#define CHECK_CLEAN(c, what) \
	CHECK(!(c)->foreign && !(c)->misrouted && !(c)->unknown, \
	      "%s: %d own frames, %d of the other client, %d other", what, (c)->foreign, (c)->misrouted, \
	      (c)->unknown)

/*
 * Tests
 */

// One client: own queries in the gaps of its traffic.
static void test_own_queries(struct mh_router *router) {
	struct client *c = &cl[0];
	const struct mhc_radio_info *p;
	int own;

	// Client idle: mhuxd asks, the client gets nothing of it.
	run_for(1.0);
	p = last_pub();
	CHECK(p && p->rxFreq == 14074000 && p->mode == MOD_DIGITAL, "own queries: 14074000 PKTUSB");
	CHECK(own_cmds >= 2, "own queries sent: %d", own_cmds);
	CHECK_CLEAN(c, "client idle");

	// Front panel change without transceive: found by the own queries.
	panel_mode(0x03, 0);
	run_for(1.2);
	p = last_pub();
	CHECK(p && p->mode == MOD_CW, "front panel change to CW found, keyer mode %d", p ? p->mode : -9);

	// Client polling faster than values go stale: no own queries, every command answered.
	client_poll(c, 0.3);
	run_for(0.7);
	own = own_cmds;
	client_reset(c);
	run_for(2.0);
	CHECK(own_cmds == own, "no own queries while the client polls, got %d", own_cmds - own);
	CHECK(c->done >= 10 && !c->timeouts, "client polling: %d answered, %d timeouts", c->done, c->timeouts);
	CHECK_CLEAN(c, "client polling");

	// Client polling slower: own queries in the gaps, the client doesn't notice.
	client_poll(c, 1.0);
	run_for(0.3);
	own = own_cmds;
	client_reset(c);
	panel_mode(0x01, 1);
	run_for(4.0);
	CHECK(own_cmds > own, "own queries between slow client polls: %d", own_cmds - own);
	CHECK(c->done >= 6 && !c->timeouts, "slow client polling: %d answered, %d timeouts", c->done, c->timeouts);
	CHECK_CLEAN(c, "slow client polling");
	CHECK(c->max_latency < 0.2, "slow client polling: max latency %.3f s", c->max_latency);

	// Client back to back: no gap for own queries.
	client_poll(c, 0);
	run_for(0.5);
	own = own_cmds;
	client_reset(c);
	run_for(1.5);
	CHECK(own_cmds == own, "no own queries between back to back client commands, got %d", own_cmds - own);
	CHECK(c->done >= 20 && !c->timeouts, "back to back: %d answered, %d timeouts", c->done, c->timeouts);
	CHECK_CLEAN(c, "back to back");

	// Back to back, but only the S-meter: no quiet gap, and the client keeps nothing fresh. Own
	// queries go in right after its answers, one at a time.
	client_poll(c, -1);
	run_for(0.1);
	client_set_poll(c, (const uint8_t[]){ 0x15, 0x02 }, 2);
	client_poll(c, 0);
	run_for(0.3);
	own = own_cmds;
	client_reset(c);
	panel_mode(0x01, 0);
	run_for(2.0);
	p = last_pub();
	CHECK(own_cmds - own >= 2, "own queries between back to back S-meter polls: %d", own_cmds - own);
	CHECK(p && p->mode == MOD_VOICE, "front panel change found next to S-meter polls, keyer mode %d",
	      p ? p->mode : -9);
	CHECK(c->done >= 20 && !c->timeouts && c->max_latency < 0.1,
	      "S-meter back to back: %d answered, %d timeouts, max latency %.3f s", c->done, c->timeouts,
	      c->max_latency);
	CHECK_CLEAN(c, "S-meter back to back");
	client_poll(c, -1);
	run_for(0.1);
	client_set_poll(c, (const uint8_t[]){ 0x03 }, 1);
	client_add_poll(c, (const uint8_t[]){ 0x26, 0x00 }, 2);

	// Transceive broadcasts during own queries reach the client.
	client_poll(c, -1);
	rig.transceive = 1;
	run_for(0.7);
	client_reset(c);
	for(int i = 0; i < 10; i++) {
		panel_mode(i & 1 ? 0x03 : 0x01, 0);
		run_for(0.13);
	}
	run_for(0.3);
	CHECK(c->transceive == 10, "transceive: %d of 10 broadcasts", c->transceive);
	CHECK_CLEAN(c, "transceive");

	// The client doesn't take transceive (forward_unsolicited false, as for Hamlib): mhuxd still
	// decodes the broadcasts, the client gets none, its polls are answered.
	mhr_set_consumer_flags(router, c->fd[0], MH_CHANNEL_R1, MHR_CNS_NO_UNSOLICITED);
	client_poll(c, 0.2);
	run_for(0.3);
	client_reset(c);
	for(int i = 0; i < 10; i++) {
		panel_mode(i & 1 ? 0x01 : 0x03, 0);
		run_for(0.13);
	}
	run_for(0.3);
	p = last_pub();
	CHECK(!c->transceive, "no transceive: %d broadcasts reached the client", c->transceive);
	CHECK(c->done >= 8 && !c->timeouts, "no transceive, polling: %d answered, %d timeouts", c->done,
	      c->timeouts);
	CHECK_CLEAN(c, "no transceive");
	CHECK(p && p->mode == MOD_VOICE, "no transceive: front panel changes still decoded, keyer mode %d",
	      p ? p->mode : -9);

	// Taken again: broadcasts reach the client.
	mhr_set_consumer_flags(router, c->fd[0], MH_CHANNEL_R1, 0);
	client_poll(c, -1);
	run_for(0.3);
	client_reset(c);
	for(int i = 0; i < 3; i++) {
		panel_mode(i & 1 ? 0x01 : 0x03, 0);
		run_for(0.13);
	}
	run_for(0.3);
	CHECK(c->transceive == 3, "transceive again: %d of 3 broadcasts", c->transceive);
	CHECK_CLEAN(c, "transceive again");
	rig.transceive = 0;
	CHECK(!collisions, "one client: %d collisions on the bus", collisions);
}

// Two clients on two connectors.
static void test_two_clients(struct mh_router *router) {
	struct client *a = &cl[0], *b = &cl[1];
	int own;

	mhr_add_endpoint_fd(router, b->fd[0], MH_CHANNEL_R1, "cat1_2");
	run_for(0.3);
	collisions = 0;

	// Both back to back: they take turns, nothing collides, each gets only its own answers.
	client_reset(a);
	client_reset(b);
	client_poll(a, 0);
	client_poll(b, 0);
	run_for(2.0);
	CHECK(a->done >= 20 && b->done >= 20, "both back to back: %d and %d answered", a->done, b->done);
	CHECK(abs(a->done - b->done) <= a->done / 5, "taking turns: %d vs %d answered", a->done, b->done);
	CHECK(!a->timeouts && !b->timeouts, "both back to back: %d and %d timeouts", a->timeouts, b->timeouts);
	CHECK_CLEAN(a, "both back to back, client A");
	CHECK_CLEAN(b, "both back to back, client B");
	CHECK(!collisions, "both back to back: %d collisions on the bus", collisions);

	// A back to back, B slow: B waits for one of A's commands at most.
	client_poll(b, 0.5);
	run_for(0.3);
	client_reset(a);
	client_reset(b);
	run_for(2.0);
	CHECK(b->done >= 6 && b->max_latency < 0.15, "slow B next to A: %d answered, max latency %.3f s",
	      b->done, b->max_latency);
	CHECK(!a->timeouts && !b->timeouts, "slow B next to A: %d and %d timeouts", a->timeouts, b->timeouts);
	CHECK_CLEAN(a, "slow B next to A, client A");
	CHECK_CLEAN(b, "slow B next to A, client B");

	// Both back to back with meters only: own queries go in between their turns.
	client_poll(a, -1);
	client_poll(b, -1);
	run_for(0.1);
	client_set_poll(a, (const uint8_t[]){ 0x15, 0x02 }, 2);
	client_set_poll(b, (const uint8_t[]){ 0x15, 0x11 }, 2);
	client_poll(a, 0);
	client_poll(b, 0);
	run_for(0.3);
	own = own_cmds;
	client_reset(a);
	client_reset(b);
	panel_mode(0x01, 1);
	run_for(2.0);
	CHECK(own_cmds - own >= 2, "own queries between two clients' meter polls: %d", own_cmds - own);
	CHECK(last_pub() && last_pub()->mode == MOD_DIGITAL, "front panel change found next to two meter pollers");
	CHECK(a->done >= 15 && b->done >= 15 && !a->timeouts && !b->timeouts,
	      "meters back to back: %d and %d answered, %d and %d timeouts", a->done, b->done, a->timeouts,
	      b->timeouts);
	CHECK(a->max_latency < 0.15 && b->max_latency < 0.15, "meters back to back: max latency %.3f and %.3f s",
	      a->max_latency, b->max_latency);
	CHECK_CLEAN(a, "meters back to back, client A");
	CHECK_CLEAN(b, "meters back to back, client B");
	client_poll(a, -1);
	client_poll(b, -1);
	run_for(0.1);
	client_set_poll(a, (const uint8_t[]){ 0x03 }, 1);
	client_add_poll(a, (const uint8_t[]){ 0x26, 0x00 }, 2);

	// Both idle: own queries again, neither client sees them. Transceive reaches both.
	client_poll(a, -1);
	client_poll(b, -1);
	run_for(0.3);
	own = own_cmds;
	client_reset(a);
	client_reset(b);
	panel_mode(0x03, 0);
	run_for(1.5);
	CHECK(own_cmds > own, "own queries with both clients idle: %d", own_cmds - own);
	CHECK(last_pub() && last_pub()->mode == MOD_CW, "front panel change found with two clients");
	rig.transceive = 1;
	for(int i = 0; i < 6; i++) {
		panel_mode(i & 1 ? 0x03 : 0x01, 0);
		run_for(0.13);
	}
	run_for(0.3);
	rig.transceive = 0;
	CHECK(a->transceive == 6 && b->transceive == 6, "transceive: %d and %d of 6 broadcasts", a->transceive,
	      b->transceive);
	CHECK_CLEAN(a, "idle, client A");
	CHECK_CLEAN(b, "idle, client B");

	// A doesn't take transceive, B does.
	mhr_set_consumer_flags(router, a->fd[0], MH_CHANNEL_R1, MHR_CNS_NO_UNSOLICITED);
	client_reset(a);
	client_reset(b);
	rig.transceive = 1;
	for(int i = 0; i < 4; i++) {
		panel_mode(i & 1 ? 0x03 : 0x01, 0);
		run_for(0.13);
	}
	run_for(0.3);
	rig.transceive = 0;
	CHECK(!a->transceive && b->transceive == 4, "transceive to B only: %d and %d of 4 broadcasts",
	      a->transceive, b->transceive);
	mhr_set_consumer_flags(router, a->fd[0], MH_CHANNEL_R1, 0);

	// B's command isn't answered: A waits until mhuxd gives up on it (CATN_TURN_TIMEOUT).
	client_poll(a, 0);
	run_for(0.3);
	client_reset(a);
	client_reset(b);
	b->npolls = 0;
	client_add_poll(b, (const uint8_t[]){ SILENT_CMD }, 1);
	client_poll(b, 5.0);
	run_for(1.5);
	CHECK(b->timeouts == 1, "silent command: %d timeouts at B", b->timeouts);
	CHECK(!a->timeouts && a->max_latency < 0.8, "A next to the silent command: %d timeouts, max latency %.3f s",
	      a->timeouts, a->max_latency);
	CHECK_CLEAN(a, "silent command, client A");

	// B's connector removed while B waits for its answer: A goes on.
	client_poll(b, -1);
	b->npolls = 0;
	client_add_poll(b, (const uint8_t[]){ 0x04 }, 1);
	client_poll(b, 0);
	run_for(0.3);
	client_reset(a);
	b->ival = -1;
	mhr_rem_endpoint_fd(router, b->fd[0], MH_CHANNEL_R1);
	b->fd[0] = -1;
	ev_io_stop(loop, &b->w);
	run_for(1.0);
	CHECK(a->done >= 10 && !a->timeouts, "A after B's connector was removed: %d answered, %d timeouts",
	      a->done, a->timeouts);
	CHECK_CLEAN(a, "B removed, client A");
	CHECK(!collisions, "two clients: %d collisions on the bus", collisions);
	client_poll(a, -1);
	run_for(0.3);
}

// The VFO knob turned with transceive on: commands collide with the broadcasts.
static void test_collisions(struct device *dev, struct cat_native **cn) {
	struct client *a = &cl[0], *b = &cl[1];
	struct cat_native_cfg cfg = { .radio = 1, .own_queries = 1, .civ_resend = 1 };
	const struct mhc_radio_info *p;
	int n;

	// B on a new connector next to A, both polling every 100 ms like Hamlib.
	ev_timer_stop(loop, &b->timer);
	close(b->fd[1]);
	b->busy = 0;
	b->rx_len = 0;
	if(client_init(b)) {
		CHECK(0, "socketpair");
		return;
	}
	client_set_poll(b, (const uint8_t[]){ 0x04 }, 1);
	client_add_poll(b, (const uint8_t[]){ 0x1a, 0x06 }, 2);
	mhr_add_endpoint_fd(dev->router, b->fd[0], MH_CHANNEL_R1, "cat1_3");
	client_poll(a, 0.1);
	client_poll(b, 0.1);
	run_for(0.3);

	// Collision resend on: the destroyed commands are sent again, the clients see neither the
	// jam nor a timeout.
	rig.transceive = 1;
	collisions = jams = 0;
	client_reset(a);
	client_reset(b);
	ev_timer_init(&tune_timer, tune_cb, 0.03, 0.03);
	ev_timer_start(loop, &tune_timer);
	run_for(4.0);
	ev_timer_stop(loop, &tune_timer);
	// The last broadcast may have been cut off: the clients' next polls tell the frequency.
	run_for(1.0);
	p = last_pub();
	CHECK(jams >= 4, "tuning: %d collisions with broadcasts", jams);
	CHECK(!a->timeouts && !b->timeouts && a->done >= 30 && b->done >= 30,
	      "resend: %d and %d answered, %d and %d timeouts", a->done, b->done, a->timeouts, b->timeouts);
	CHECK_CLEAN(a, "resend, client A");
	CHECK_CLEAN(b, "resend, client B");
	CHECK(!collisions, "resend: %d commands while an answer was due", collisions);
	CHECK(p && p->rxFreq == rig.freq, "resend: frequency followed the knob, %u vs %u", p ? p->rxFreq : 0,
	      rig.freq);

	// Resend off: the same collisions make the clients time out.
	cat_native_destroy(*cn);
	cfg.civ_resend = 0;
	*cn = cat_native_create((app_ctx *)dev, dev, &cfg);
	run_for(0.3);
	jams = 0;
	client_reset(a);
	client_reset(b);
	ev_timer_start(loop, &tune_timer);
	run_for(4.0);
	ev_timer_stop(loop, &tune_timer);
	run_for(1.2);
	n = jams;
	CHECK(n >= 4 && a->timeouts + b->timeouts >= n / 2,
	      "no resend: %d collisions, %d and %d timeouts", n, a->timeouts, b->timeouts);

	cat_native_destroy(*cn);
	cfg.civ_resend = 1;
	*cn = cat_native_create((app_ctx *)dev, dev, &cfg);
	rig.transceive = 0;
	client_poll(a, -1);
	client_poll(b, -1);
	run_for(1.2);
}

int main(void) {
	struct device dev = { .ctl = (struct mh_control *)&dev, .serial = "TEST0001" };
	struct cat_native_cfg cfg = { .radio = 1, .own_queries = 1, .civ_resend = 1 };

	loop = ev_default_loop(0);
	if(nonblock_pair(kfd) || client_init(&cl[0]) || client_init(&cl[1])) {
		printf("FAIL router_test: socketpair\n");
		return 1;
	}
	client_add_poll(&cl[0], (const uint8_t[]){ 0x03 }, 1);
	client_add_poll(&cl[0], (const uint8_t[]){ 0x26, 0x00 }, 2);
	client_add_poll(&cl[1], (const uint8_t[]){ 0x04 }, 1);
	client_add_poll(&cl[1], (const uint8_t[]){ 0x1a, 0x06 }, 2);

	dev.router = mhr_create(loop, "TEST0001", 0);
	mhr_set_keyer_fd(dev.router, kfd[0]);
	mhr_add_endpoint_fd(dev.router, cl[0].fd[0], MH_CHANNEL_R1, "cat1");
	ev_io_init(&keyer_w, keyer_cb, kfd[1], EV_READ);
	ev_io_start(loop, &keyer_w);
	ev_timer_init(&outq_timer, outq_cb, 0.002, 0.002);
	ev_timer_start(loop, &outq_timer);

	speed.rigtype = RIG_IC7300;
	speed.icomaddress = RADIO_ADDR;
	speed_valid = 1;
	struct cat_native *cn = cat_native_create((app_ctx *)&dev, &dev, &cfg);

	test_own_queries(dev.router);
	test_two_clients(dev.router);
	test_collisions(&dev, &cn);

	cat_native_destroy(cn);
	mhr_destroy(dev.router);
	close(kfd[1]);
	for(int i = 0; i < NCLIENTS; i++)
		close(cl[i].fd[1]);
	printf("%s router_test: radio got %d own queries, %d client commands\n", failures ? "FAIL" : "ok  ",
	       own_cmds, client_cmds);
	return failures ? 1 : 0;
}
