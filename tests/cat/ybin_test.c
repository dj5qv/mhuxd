/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Native CAT with the older Yaesu radios (binary protocol, src/cat_ybin.c) end
 * to end, through the real router: an emulated keyer with a radio emulated by
 * ybin_radio.c behind it, at 4800 Bd, and client applications on connector
 * socketpairs that poll like Hamlib: a command, then as many bytes as its
 * answer has. The answers have no framing, so a byte of mhuxd's own queries or
 * of another client's answer that reaches a client shows up as a wrong count.
 *
 * FT-817: own queries while the client is idle or polls slowly, none while it
 * polls fast, acknowledged sets. FT-1000MP: two clients taking turns, a memory
 * dump of 1863 bytes (4 s) that keeps the line, answers slowed by pacing.
 * FT-757GXII: own queries with its 75 byte status data. Hamlib's FT-1000MP
 * backend (rigctl, if installed) on a pseudo terminal as a connector, next to
 * a polling client.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "cat_native.h"
#include "app_ctx.h"
#include "channel.h"
#include "device.h"
#include "mhrouter.h"
#include "stubs.h"
#include "ybin_radio.h"

#define RIG_FT1000MP 0x21
#define RIG_FT757GX2 0x45

#define RADIO_DELAY 0.02	// the radio's processing time before an answer
#define BYTE_TIME 0.0023	// 4800 Bd, 8N2

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
 * Keyer and radio
 */

static int kfd[2] = { -1, -1 };
static ev_io keyer_w;
static struct keyer_rx krx = { -1, 0 };
static struct ybin_radio *rig;
static int collisions;		// commands while the radio was still answering
static ev_tstamp answer_end;	// last byte of the answer in progress
static int radio_cmds;		// complete commands the radio got

// Radio output, byte by byte, sent when due.
#define OUTQ_MAX 4096
static struct {
	ev_tstamp due;
	uint8_t b;
} outq[OUTQ_MAX];
static int outq_head, outq_len;
static ev_timer outq_timer;

static void outq_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	ev_tstamp now = ev_now(l);
	uint8_t f[4 * 64];
	size_t n = 0;
	(void)w; (void)revents;

	while(outq_len && outq[outq_head].due <= now && n < sizeof(f)) {
		keyer_r1_encode(outq[outq_head].b, f + n);
		n += 4;
		outq_head = (outq_head + 1) % OUTQ_MAX;
		outq_len--;
	}
	if(n && write(kfd[1], f, n) != (ssize_t)n)
		CHECK(0, "keyer write: %s", strerror(errno));
}

static void radio_rx(uint8_t b) {
	static uint8_t out[2048];
	ev_tstamp now = ev_now(loop);
	int before = ybr_stats(rig)->commands;
	size_t n = ybr_rx(rig, b, now, out);

	if(ybr_stats(rig)->commands == before)
		return;
	radio_cmds++;
	if(answer_end > now)
		collisions++;
	ev_tstamp t = now + RADIO_DELAY;
	for(size_t i = 0; i < n && outq_len < OUTQ_MAX; i++) {
		int idx = (outq_head + outq_len++) % OUTQ_MAX;
		outq[idx].due = t;
		outq[idx].b = out[i];
		answer_end = t;
		t += BYTE_TIME + ybr_state(rig)->pacing_ms / 1000.0;
	}
}

static void keyer_cb(struct ev_loop *l, struct ev_io *w, int revents) {
	uint8_t buf[256], b;
	ssize_t r = read(w->fd, buf, sizeof(buf));
	(void)l; (void)revents;

	for(ssize_t i = 0; i < r; i++)
		if(keyer_r1_decode(&krx, buf[i], &b))
			radio_rx(b);
}

static void radio_use(const char *model) {
	ybr_destroy(rig);
	rig = ybr_create(model);
	outq_len = 0;
	answer_end = 0;
}

/*
 * Client applications: one command at a time, then as many bytes as its answer has.
 */

#define NCLIENTS 2
#define MAX_POLLS 4

struct poll_cmd {
	uint8_t cmd[5];
	size_t answer;
};

struct client {
	int fd[2];
	ev_io w;
	ev_timer timer;
	struct poll_cmd polls[MAX_POLLS];
	int npolls;
	double ival;		// poll interval, 0 = back to back, < 0 = idle
	double timeout;
	int step;
	int busy;
	ev_tstamp sent;
	uint8_t rx[2048];
	size_t got;
	// results
	int done, timeouts, sent_cmds;
	int extra;		// bytes beyond the answer's length
	int late;		// bytes while no answer was due
	double max_latency;
};

static struct client cl[NCLIENTS];
static int client_total;	// commands the clients sent, all tests

static void client_set_poll(struct client *c, const char *hex, size_t answer) {
	c->npolls = 0;
	parse_hex(hex, c->polls[0].cmd);
	c->polls[0].answer = answer;
	c->npolls = 1;
}

static void client_add_poll(struct client *c, const char *hex, size_t answer) {
	parse_hex(hex, c->polls[c->npolls].cmd);
	c->polls[c->npolls++].answer = answer;
}

static void client_next(struct client *c);

static void client_send(struct client *c) {
	const struct poll_cmd *q = &c->polls[c->step];
	c->sent = ev_now(loop);
	c->got = 0;
	c->sent_cmds++;
	client_total++;
	if(write(c->fd[1], q->cmd, 5) != 5)
		CHECK(0, "client write: %s", strerror(errno));
	ev_timer_stop(loop, &c->timer);
	if(!q->answer) {
		// A set without answer: Hamlib waits a moment (post_write_delay).
		c->busy = 0;
		ev_timer_set(&c->timer, 0.05, 0.);
	} else {
		c->busy = 1;
		ev_timer_set(&c->timer, c->timeout, 0.);
	}
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

static void client_cb(struct ev_loop *l, struct ev_io *w, int revents) {
	struct client *c = w->data;
	uint8_t buf[512];
	ssize_t r = read(w->fd, buf, sizeof(buf));
	(void)l; (void)revents;

	for(ssize_t i = 0; i < r; i++) {
		if(!c->busy) {
			c->late++;
			continue;
		}
		if(c->got < sizeof(c->rx))
			c->rx[c->got] = buf[i];
		if(++c->got == c->polls[c->step].answer) {
			client_done(c);
			// The rest of this read: bytes no answer was due for.
			c->extra += r - i - 1 > 0 && !c->busy ? r - i - 1 : 0;
			if(!c->busy)
				break;
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
	c->done = c->timeouts = c->sent_cmds = c->extra = c->late = 0;
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
	c->timeout = 1.0;
	ev_io_init(&c->w, client_cb, c->fd[1], EV_READ);
	c->w.data = c;
	ev_io_start(loop, &c->w);
	ev_timer_init(&c->timer, client_timer_cb, 0., 0.);
	c->timer.data = c;
	return 0;
}

// Exactly the answers to its commands reached the client.
#define CHECK_CLEAN(c, what) \
	CHECK(!(c)->extra && !(c)->late, "%s: %d bytes beyond the answers, %d while none was due", what, \
	      (c)->extra, (c)->late)

static void stop_clients(void) {
	for(int i = 0; i < NCLIENTS; i++)
		client_poll(&cl[i], -1);
	run_for(0.5);
}

/*
 * Tests
 */

static struct cat_native *restart(struct device *dev, struct cat_native *cn, int rigtype, const char *model) {
	struct cat_native_cfg cfg = { .radio = 1, .own_queries = 1 };

	cat_native_destroy(cn);
	radio_use(model);
	speed.rigtype = rigtype;
	for(int i = 0; i < NCLIENTS; i++)
		client_reset(&cl[i]);
	collisions = 0;
	return cat_native_create((app_ctx *)dev, dev, &cfg);
}

static void test_ft817(void) {
	struct client *c = &cl[0];
	struct ybr_state *s = ybr_state(rig);
	const struct mhc_radio_info *p;
	int own;

	// Client idle: mhuxd asks with 00 00 00 00 03, the client gets nothing of it.
	run_for(1.0);
	p = last_pub();
	CHECK(p && p->rxFreq == 14074000 && p->mode == MOD_VOICE, "FT-817 own queries: 14074000 USB, got %u %d",
	      p ? p->rxFreq : 0, p ? p->mode : -9);
	CHECK(radio_cmds >= 1, "FT-817 own queries sent: %d", radio_cmds);
	CHECK_CLEAN(c, "FT-817 client idle");

	// Front panel: DIG on 40 m, found by the own queries.
	s->freq[0] = 7074000;
	s->mode[0] = YBR_DIG;
	run_for(1.2);
	p = last_pub();
	CHECK(p && p->rxFreq == 7074000 && p->mode == MOD_DIGITAL, "FT-817 front panel change found: %u %d",
	      p ? p->rxFreq : 0, p ? p->mode : -9);

	// Client polling frequency and mode and the RX status every 300 ms: no own queries, every
	// command answered, nothing else reaches the client.
	client_set_poll(c, "00 00 00 00 03", 5);
	client_add_poll(c, "00 00 00 00 e7", 1);
	client_poll(c, 0.3);
	run_for(0.7);
	client_reset(c);
	own = radio_cmds;
	run_for(2.0);
	own = radio_cmds - own - c->sent_cmds;
	CHECK(own <= 0, "FT-817: no own queries while the client polls, got %d", own);
	CHECK(c->done >= 10 && !c->timeouts, "FT-817 polling: %d answered, %d timeouts", c->done, c->timeouts);
	CHECK_CLEAN(c, "FT-817 polling");

	// Slow polling: own queries in the gaps, the client doesn't notice.
	client_poll(c, 1.2);
	run_for(0.3);
	client_reset(c);
	own = radio_cmds;
	s->mode[0] = YBR_CW;
	run_for(4.0);
	own = radio_cmds - own - c->sent_cmds;
	CHECK(own >= 1, "FT-817: own queries between slow polls: %d", own);
	CHECK(c->done >= 4 && !c->timeouts && c->max_latency < 0.2, "FT-817 slow polling: %d answered, "
	      "%d timeouts, max latency %.3f s", c->done, c->timeouts, c->max_latency);
	CHECK_CLEAN(c, "FT-817 slow polling");
	p = last_pub();
	CHECK(p && p->mode == MOD_CW, "FT-817: CW found next to slow polls, keyer mode %d", p ? p->mode : -9);

	// Sets, acknowledged with one byte: taken over at once.
	client_poll(c, -1);
	run_for(0.3);
	client_reset(c);
	client_set_poll(c, "01 42 50 00 01", 1);	// 14.250 MHz
	client_add_poll(c, "01 00 00 00 07", 1);	// USB
	client_poll(c, 0);
	run_for(0.15);
	client_poll(c, -1);
	run_for(0.4);
	p = last_pub();
	CHECK(p && p->rxFreq == 14250000 && p->mode == MOD_VOICE, "FT-817 sets: %u %d", p ? p->rxFreq : 0,
	      p ? p->mode : -9);
	CHECK(s->freq[0] == 14250000 && s->mode[0] == YBR_USB, "FT-817 sets reached the radio");
	CHECK(!c->timeouts && c->done >= 2, "FT-817 sets: %d acknowledged, %d timeouts", c->done, c->timeouts);
	CHECK_CLEAN(c, "FT-817 sets");
}

static void test_ft1000mp(struct mh_router *router) {
	struct client *a = &cl[0], *b = &cl[1];
	struct ybr_state *s = ybr_state(rig);
	const struct mhc_radio_info *p;

	// Idle: own queries (10, current data) find the radio's state.
	s->freq[0] = 21074000;
	s->mode[0] = YBR_PKTL;
	run_for(1.0);
	p = last_pub();
	CHECK(p && p->rxFreq == 21074000 && p->mode == MOD_DIGITAL, "FT-1000MP own queries: %u %d",
	      p ? p->rxFreq : 0, p ? p->mode : -9);

	// Two clients back to back on two connectors: they take turns, each gets its answers.
	mhr_add_endpoint_fd(router, b->fd[0], MH_CHANNEL_R1, "cat1_2");
	client_set_poll(a, "00 00 00 03 10", 32);	// VFO A and B
	client_add_poll(a, "00 00 00 01 fa", 6);	// status flags
	client_set_poll(b, "00 00 00 02 10", 16);	// current data
	client_add_poll(b, "00 00 00 00 f7", 5);	// meter
	client_poll(a, 0);
	client_poll(b, 0);
	run_for(0.5);
	client_reset(a);
	client_reset(b);
	collisions = 0;
	s->split = 1;
	run_for(3.0);
	p = last_pub();
	CHECK(!collisions, "FT-1000MP two clients: %d commands while an answer was due", collisions);
	CHECK(a->done >= 15 && b->done >= 15 && !a->timeouts && !b->timeouts,
	      "FT-1000MP two clients: %d and %d answered, %d and %d timeouts", a->done, b->done, a->timeouts,
	      b->timeouts);
	CHECK_CLEAN(a, "FT-1000MP client A");
	CHECK_CLEAN(b, "FT-1000MP client B");
	CHECK(p && p->txFreq == s->freq[1], "FT-1000MP split from the status flags: tx %u", p ? p->txFreq : 0);
	stop_clients();

	// All data (1863 bytes, 4 s at 4800 Bd) to A. B's command meanwhile waits for it, no own
	// query goes in between, each client gets its own answer.
	client_reset(a);
	client_reset(b);
	collisions = 0;
	a->timeout = 6.0;
	b->timeout = 6.0;
	client_set_poll(a, "00 00 00 00 10", 1863);
	client_poll(a, 100);
	run_for(1.0);
	client_set_poll(b, "00 00 00 02 10", 16);
	client_poll(b, 100);
	run_for(4.5);
	CHECK(a->done == 1 && !a->timeouts && a->got == 1863, "FT-1000MP dump: %d answered, %zu bytes", a->done,
	      a->got);
	int pattern = 1;
	for(int i = 0; i < 1863; i++)
		if(a->rx[i < (int)sizeof(a->rx) ? i : 0] != (i & 0xff) && i < (int)sizeof(a->rx))
			pattern = 0;
	CHECK(pattern, "FT-1000MP dump: the bytes as the radio sent them");
	CHECK(b->done == 1 && !b->timeouts && b->max_latency > 2.0,
	      "FT-1000MP command next to the dump: %d answered after %.1f s", b->done, b->max_latency);
	CHECK(!collisions, "FT-1000MP dump: %d commands while an answer was due", collisions);
	CHECK_CLEAN(a, "FT-1000MP dump, client A");
	CHECK_CLEAN(b, "FT-1000MP dump, client B");
	stop_clients();

	// Pacing 40 ms: a 16 byte answer takes 0.7 s, more than mhuxd's turn timeout. The turn
	// lasts while the bytes come.
	a->timeout = b->timeout = 2.0;
	client_set_poll(a, "00 00 00 28 0e", 0);
	client_poll(a, 100);
	run_for(0.3);
	client_reset(a);
	client_reset(b);
	collisions = 0;
	client_set_poll(a, "00 00 00 02 10", 16);
	client_set_poll(b, "00 00 00 00 f7", 5);
	client_poll(a, 0);
	client_poll(b, 0);
	run_for(4.0);
	CHECK(!collisions, "FT-1000MP pacing: %d commands while an answer was due", collisions);
	CHECK(a->done >= 2 && b->done >= 2 && !a->timeouts && !b->timeouts,
	      "FT-1000MP pacing: %d and %d answered, %d and %d timeouts", a->done, b->done, a->timeouts,
	      b->timeouts);
	CHECK_CLEAN(a, "FT-1000MP pacing, client A");
	CHECK_CLEAN(b, "FT-1000MP pacing, client B");
	stop_clients();
	client_set_poll(a, "00 00 00 00 0e", 0);
	client_poll(a, 100);
	run_for(0.3);
	client_poll(a, -1);
	mhr_rem_endpoint_fd(router, b->fd[0], MH_CHANNEL_R1);
}

static int child_status = -1;

// libev's default loop reaps the children itself.
static void child_cb(struct ev_loop *l, ev_child *w, int revents) {
	(void)revents;
	child_status = w->rstatus;
	ev_child_stop(l, w);
}

// rigctl -m 1024 (FT-1000MP) on a pseudo terminal connector, client B polling on another one.
static void test_hamlib(struct mh_router *router) {
	struct client *b = &cl[1];
	struct ybr_state *s = ybr_state(rig);
	char out[512];
	size_t len = 0;
	int status, out_fd[2];
	pid_t pid;
	ev_child cw;

	int m = posix_openpt(O_RDWR | O_NOCTTY);
	if(m < 0 || grantpt(m) || unlockpt(m) || pipe(out_fd)) {
		CHECK(0, "pseudo terminal: %s", strerror(errno));
		return;
	}
	fcntl(m, F_SETFL, O_NONBLOCK);
	fcntl(out_fd[0], F_SETFL, O_NONBLOCK);
	s->freq[0] = 14074000;
	s->mode[0] = YBR_USB;
	s->freq[1] = 14250000;
	s->vfo = 0;
	s->split = 0;
	// B's connector was removed (and closed) by the FT-1000MP test: a new one.
	ev_io_stop(loop, &b->w);
	ev_timer_stop(loop, &b->timer);
	close(b->fd[1]);
	b->busy = 0;
	if(client_init(b)) {
		CHECK(0, "socketpair");
		return;
	}
	mhr_add_endpoint_fd(router, m, MH_CHANNEL_R1, "cat1_3");
	mhr_add_endpoint_fd(router, b->fd[0], MH_CHANNEL_R1, "cat1_2");
	client_reset(b);
	collisions = 0;
	b->timeout = 1.0;
	client_set_poll(b, "00 00 00 00 f7", 5);
	client_poll(b, 0.2);

	pid = fork();
	if(pid == 0) {
		dup2(out_fd[1], 1);
		dup2(out_fd[1], 2);
		execlp("rigctl", "rigctl", "-m", "1024", "-r", ptsname(m), "-s", "4800", "f", "m", "F", "7074000",
		       "M", "PKTLSB", "0", "f", "m", "s", (char *)NULL);
		_exit(127);
	}
	close(out_fd[1]);
	child_status = -1;
	ev_child_init(&cw, child_cb, pid, 0);
	ev_child_start(loop, &cw);
	for(int i = 0; i < 300 && pid > 0 && child_status < 0; i++) {
		ssize_t r;
		run_for(0.1);
		while((r = read(out_fd[0], out + len, sizeof(out) - 1 - len)) > 0)
			len += r;
	}
	ev_child_stop(loop, &cw);
	status = child_status;
	out[len] = '\0';
	close(out_fd[0]);
	stop_clients();
	mhr_rem_endpoint_fd(router, m, MH_CHANNEL_R1);
	mhr_rem_endpoint_fd(router, b->fd[0], MH_CHANNEL_R1);
	close(m);

	if(WIFEXITED(status) && WEXITSTATUS(status) == 127) {
		printf("skip ybin_test: no rigctl\n");
		return;
	}
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "rigctl FT-1000MP: exit status %d", status);
	CHECK(!strcmp(out, "14074000\nUSB\n2400\n7074000\nPKTLSB\n2400\n0\nVFOA\n"),
	      "rigctl FT-1000MP through mhuxd: '%s'", out);
	CHECK(s->freq[0] == 7074000 && s->mode[0] == YBR_PKTL, "rigctl FT-1000MP sets reached the radio");
	CHECK(!collisions, "rigctl FT-1000MP: %d commands while an answer was due", collisions);
	CHECK(b->done >= 2 && !b->timeouts, "client next to rigctl: %d answered, %d timeouts", b->done,
	      b->timeouts);
	CHECK_CLEAN(b, "client next to rigctl");
}

static void test_ft757(void) {
	struct ybr_state *s = ybr_state(rig);
	const struct mhc_radio_info *p;

	// 75 bytes of status data for the own query, 0.2 s at 4800 Bd.
	s->freq[0] = 3573000;
	s->mode[0] = YBR_CW;
	run_for(1.0);
	p = last_pub();
	CHECK(p && p->rxFreq == 3573000 && p->mode == MOD_CW, "FT-757GXII own queries: %u %d",
	      p ? p->rxFreq : 0, p ? p->mode : -9);
	CHECK_CLEAN(&cl[0], "FT-757GXII client idle");
}

int main(void) {
	struct device dev = { .ctl = (struct mh_control *)&dev, .serial = "TEST0001" };
	struct cat_native *cn = NULL;

	loop = ev_default_loop(0);
	signal(SIGPIPE, SIG_IGN);
	if(nonblock_pair(kfd) || client_init(&cl[0]) || client_init(&cl[1])) {
		printf("FAIL ybin_test: socketpair\n");
		return 1;
	}
	dev.router = mhr_create(loop, "TEST0001", 0);
	mhr_set_keyer_fd(dev.router, kfd[0]);
	mhr_add_endpoint_fd(dev.router, cl[0].fd[0], MH_CHANNEL_R1, "cat1");
	ev_io_init(&keyer_w, keyer_cb, kfd[1], EV_READ);
	ev_io_start(loop, &keyer_w);
	ev_timer_init(&outq_timer, outq_cb, 0.002, 0.002);
	ev_timer_start(loop, &outq_timer);
	speed_valid = 1;

	cn = restart(&dev, cn, RIG_FT817, "ft817");
	test_ft817();
	cn = restart(&dev, cn, RIG_FT1000MP, "ft1000mp");
	test_ft1000mp(dev.router);
	test_hamlib(dev.router);
	cn = restart(&dev, cn, RIG_FT757GX2, "ft757gx2");
	test_ft757();

	cat_native_destroy(cn);
	mhr_destroy(dev.router);
	ybr_destroy(rig);
	close(kfd[1]);
	for(int i = 0; i < NCLIENTS; i++)
		close(cl[i].fd[1]);
	printf("%s ybin_test: radio got %d commands, %d from the clients\n", failures ? "FAIL" : "ok  ",
	       radio_cmds, client_total);
	return failures ? 1 : 0;
}
