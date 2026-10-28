/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * PTT bookkeeping in the router (src/mhrouter.c): connectors write PTT bytes to the
 * PTT channels, the router keys the radios through the flags byte it sends to the
 * keyer. An emulated keyer behind a socketpair decodes that byte, the connectors are
 * socketpairs added as conmgr does it.
 *
 *   - a radio stays keyed while any connector still holds its PTT
 *   - a connector that goes away without PTT off (deleted, or its end closed) releases it
 *   - on CH_PTT_FOCUS, PTT off releases the radio that was keyed, not the focused one
 *   - after the keyer was gone, no PTT comes back that nobody keyed since
 *
 * make -C tests/ptt check, VERBOSE=1 for the router's log.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ev.h>
#include "channel.h"
#include "logger.h"
#include "mhflags.h"
#include "mhrouter.h"
#include "ptt_byte.h"
#include "util.h"

#define R1 MHC2DFL_PTT_R1
#define R2 MHC2DFL_PTT_R2

static struct ev_loop *loop;
static int failures;

#define CHECK(cond, ...) do { \
	if(!(cond)) { \
		printf("FAIL %s:%d: ", __FILE__, __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
		failures++; \
	} \
} while(0)

/*
 * Stubs for the router's dependencies
 */

int log_level = 0;
void log_msg(int severity, const char *header, const char *fmt, ...) {
	va_list ap;
	(void)severity;
	if(!getenv("VERBOSE"))
		return;
	printf("    %s ", header);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void log_hex(int severity, const char *msg1, const char *msg2, const char *msg3, const char *buf, int len) {
	(void)severity; (void)msg1; (void)msg2; (void)msg3; (void)buf; (void)len;
}

void *w_malloc(size_t size) {
	void *p = malloc(size);
	if(!p)
		abort();
	return p;
}

void *w_calloc(size_t nmemb, size_t size) {
	void *p = calloc(nmemb, size);
	if(!p)
		abort();
	return p;
}

int fd_close(int *fd) {
	if(!fd || *fd < 0)
		return 0;
	close(*fd);
	*fd = -1;
	return 1;
}

const char *ch_channel2str(int channel) {
	switch(channel) {
	case CH_PTT1:      return "PTT1";
	case CH_PTT2:      return "PTT2";
	case CH_PTT_FOCUS: return "PTT_FOCUS";
	default:           return "?";
	}
}

void tty_close(int fd) {
	close(fd);
}

/*
 * Keyer: decodes the flags byte, the shared channel of the first frame of a sequence
 * (src/mux.c). A frame is a sync byte (MSB clear, bit 6 set on all but the first frame
 * of a sequence) and three payload bytes; the shared channel's payload is the third,
 * valid if sync bit 3 is set, its MSB in sync bit 0.
 */

static int kfd[2] = { -1, -1 };
static ev_io keyer_w;
static struct {
	int sync, idx;
	int flags;	// last flags byte, -1: none yet
	int count;	// flags bytes received
} keyer = { -1, 0, -1, 0 };

static void keyer_cb(struct ev_loop *l, struct ev_io *w, int revents) {
	uint8_t buf[256];
	ssize_t r;
	(void)l; (void)revents;

	while((r = read(w->fd, buf, sizeof(buf))) > 0) {
		for(ssize_t i = 0; i < r; i++) {
			uint8_t c = buf[i];
			if(!(c & 0x80)) {
				keyer.sync = c;
				keyer.idx = 0;
				continue;
			}
			if(keyer.sync < 0 || ++keyer.idx != 3)
				continue;
			if(!(keyer.sync & 0x40) && (keyer.sync & 0x08)) {
				keyer.flags = (c & 0x7f) | ((keyer.sync & 1) << 7);
				keyer.count++;
			}
		}
	}
	if(r == 0)
		ev_io_stop(loop, w);
}

static int nonblock_pair(int fd[2]) {
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, fd))
		return -1;
	fcntl(fd[0], F_SETFL, O_NONBLOCK);
	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	return 0;
}

static void keyer_connect(struct mh_router *router) {
	if(nonblock_pair(kfd)) {
		printf("FAIL router_ptt_test: socketpair\n");
		exit(1);
	}
	keyer.sync = -1;
	ev_io_init(&keyer_w, keyer_cb, kfd[1], EV_READ);
	ev_io_start(loop, &keyer_w);
	mhr_set_keyer_fd(router, kfd[0]);
}

// The router closes its end, as when the USB connection is lost.
static void keyer_disconnect(struct mh_router *router) {
	mhr_set_keyer_fd(router, -1);
	ev_io_stop(loop, &keyer_w);
	close(kfd[1]);
	kfd[1] = -1;
}

static void stop_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	(void)w; (void)revents;
	ev_break(l, EVBREAK_ONE);
}

static void run_for(double s) {
	ev_timer t;
	ev_timer_init(&t, stop_cb, s, 0.);
	ev_timer_start(loop, &t);
	ev_run(loop, 0);
	ev_timer_stop(loop, &t);
}

// The PTT bits of the last flags byte the keyer got, -1 if it got none.
static int keyer_ptt(void) {
	run_for(0.02);
	return keyer.flags < 0 ? -1 : keyer.flags & (R1 | R2);
}

/*
 * Connectors: a socketpair, the router end added to a PTT channel.
 */

struct con {
	int fd[2];
	int channel;
	const char *tag;
};

static void con_add(struct mh_router *router, struct con *c, int channel, const char *tag) {
	if(nonblock_pair(c->fd)) {
		printf("FAIL router_ptt_test: socketpair\n");
		exit(1);
	}
	c->channel = channel;
	c->tag = tag;
	mhr_add_endpoint_fd(router, c->fd[0], channel, tag);
}

// Processed before the next one: the router reads the producers of a channel in no
// particular order, two bytes written back to back could arrive the other way round.
static void con_ptt(struct con *c, int on) {
	uint8_t b = on ? PTT_ON_BYTE : PTT_OFF_BYTE;
	if(write(c->fd[1], &b, 1) != 1)
		CHECK(0, "%s: write: %s", c->tag, strerror(errno));
	run_for(0.005);
}

// Deleted: conmgr removes the router endpoint, the router closes its end.
static void con_delete(struct mh_router *router, struct con *c) {
	mhr_rem_endpoint_fd(router, c->fd[0], c->channel);
	close(c->fd[1]);
}

// Failed or closed: the connector closes its end, the router reads EOF.
static void con_close(struct con *c) {
	close(c->fd[1]);
}

static void test_two_holders(struct mh_router *router) {
	struct con a, b;
	con_add(router, &a, CH_PTT1, "a");
	con_add(router, &b, CH_PTT1, "b");

	con_ptt(&a, 1);
	CHECK(keyer_ptt() == R1, "a keyed: ptt %d, want R1", keyer_ptt());
	con_ptt(&b, 1);
	con_ptt(&a, 0);
	CHECK(keyer_ptt() == R1, "a released, b still keys: ptt %d, want R1", keyer_ptt());
	con_ptt(&b, 0);
	CHECK(keyer_ptt() == 0, "both released: ptt %d, want 0", keyer_ptt());

	// a radio on each PTT channel
	struct con c;
	con_add(router, &c, CH_PTT2, "c");
	con_ptt(&a, 1);
	con_ptt(&c, 1);
	CHECK(keyer_ptt() == (R1 | R2), "a and c keyed: ptt %d, want R1|R2", keyer_ptt());
	con_ptt(&a, 0);
	CHECK(keyer_ptt() == R2, "a released: ptt %d, want R2", keyer_ptt());
	con_ptt(&c, 0);
	CHECK(keyer_ptt() == 0, "c released: ptt %d, want 0", keyer_ptt());

	// a repeated PTT on does not count twice
	con_ptt(&a, 1);
	con_ptt(&a, 1);
	con_ptt(&a, 0);
	CHECK(keyer_ptt() == 0, "a on twice, off once: ptt %d, want 0", keyer_ptt());

	con_delete(router, &a);
	con_delete(router, &b);
	con_delete(router, &c);
	run_for(0.02);
}

static void test_gone_without_off(struct mh_router *router) {
	struct con a, b;
	con_add(router, &a, CH_PTT1, "a");
	con_add(router, &b, CH_PTT1, "b");

	con_ptt(&a, 1);
	CHECK(keyer_ptt() == R1, "a keyed: ptt %d, want R1", keyer_ptt());
	con_delete(router, &a);
	CHECK(keyer_ptt() == 0, "a deleted while keyed: ptt %d, want 0", keyer_ptt());

	con_ptt(&b, 1);
	CHECK(keyer_ptt() == R1, "b keyed: ptt %d, want R1", keyer_ptt());
	con_close(&b);
	CHECK(keyer_ptt() == 0, "b closed while keyed: ptt %d, want 0", keyer_ptt());

	// one that goes while another still keys must not unkey
	con_add(router, &a, CH_PTT1, "a");
	con_add(router, &b, CH_PTT1, "b");
	con_ptt(&a, 1);
	con_ptt(&b, 1);
	run_for(0.02);
	con_close(&b);
	CHECK(keyer_ptt() == R1, "b closed, a still keys: ptt %d, want R1", keyer_ptt());
	con_ptt(&a, 0);
	CHECK(keyer_ptt() == 0, "a released: ptt %d, want 0", keyer_ptt());

	con_delete(router, &a);
	run_for(0.02);
}

static void test_focus(struct mh_router *router) {
	struct con w, c;
	con_add(router, &w, CH_PTT_FOCUS, "wk");
	con_add(router, &c, CH_PTT2, "c");

	mhr_set_wk_tx_focus(router, 0);
	con_ptt(&w, 1);
	CHECK(keyer_ptt() == R1, "focus R1, keyed: ptt %d, want R1", keyer_ptt());
	mhr_set_wk_tx_focus(router, 1);
	con_ptt(&w, 0);
	CHECK(keyer_ptt() == 0, "focus moved to R2, released: ptt %d, want 0", keyer_ptt());

	// PTT on again after the focus moved moves the PTT along
	mhr_set_wk_tx_focus(router, 0);
	con_ptt(&w, 1);
	mhr_set_wk_tx_focus(router, 1);
	con_ptt(&w, 1);
	CHECK(keyer_ptt() == R2, "keyed again with focus R2: ptt %d, want R2", keyer_ptt());

	// releasing R2 on the focus channel leaves R2 keyed by another connector
	con_ptt(&c, 1);
	con_ptt(&w, 0);
	CHECK(keyer_ptt() == R2, "wk released, c still keys R2: ptt %d, want R2", keyer_ptt());
	con_ptt(&c, 0);
	CHECK(keyer_ptt() == 0, "c released: ptt %d, want 0", keyer_ptt());

	con_delete(router, &w);
	con_delete(router, &c);
	run_for(0.02);
}

static void test_keyer_reconnect(struct mh_router *router) {
	struct con a, b;
	con_add(router, &a, CH_PTT1, "a");
	con_add(router, &b, CH_PTT2, "b");

	con_ptt(&a, 1);
	CHECK(keyer_ptt() == R1, "a keyed: ptt %d, want R1", keyer_ptt());

	keyer_disconnect(router);
	run_for(0.02);
	keyer_connect(router);
	keyer.flags = -1;

	con_ptt(&b, 1);
	CHECK(keyer_ptt() == R2, "after reconnect, b keyed: ptt %d, want R2 only", keyer_ptt());
	con_ptt(&b, 0);
	CHECK(keyer_ptt() == 0, "b released: ptt %d, want 0", keyer_ptt());

	// PTT on sent while the keyer was gone is processed after the reconnect
	keyer_disconnect(router);
	con_ptt(&a, 1);
	run_for(0.02);
	keyer_connect(router);
	CHECK(keyer_ptt() == R1, "a keyed while offline: ptt %d, want R1 after reconnect", keyer_ptt());
	con_ptt(&a, 0);
	CHECK(keyer_ptt() == 0, "a released: ptt %d, want 0", keyer_ptt());

	con_delete(router, &a);
	con_delete(router, &b);
	run_for(0.02);
}

int main(void) {
	loop = ev_default_loop(0);
	struct mh_router *router = mhr_create(loop, "TEST0001", 1);
	keyer_connect(router);

	test_two_holders(router);
	test_gone_without_off(router);
	test_focus(router);
	test_keyer_reconnect(router);

	mhr_destroy(router);
	if(kfd[1] != -1)
		close(kfd[1]);
	printf("%s router_ptt_test: %d flags bytes to the keyer\n", failures ? "FAIL" : "ok  ",
	       keyer.count);
	return failures ? 1 : 0;
}
