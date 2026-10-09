/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Tests the glue between the CI-V decoder and mhcontrol (src/cat_native.c):
 * publish delay, waiting for the data flag, keepalive, rig type changes, keyer
 * state, CAT monitor. Built with CATN_KEEPALIVE=0.5 (see Makefile).
 *
 *   native_test <dir>
 *
 * writes the CAT monitor file to <dir>, the Makefile replays it.
 * Router, mhcontrol, event bus and logging are stubs; the libev loop is real,
 * so the test takes a few seconds.
 */

#include <ev.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cat_native.h"
#include "app_ctx.h"
#include "device.h"
#include "eventbus.h"
#include "events.h"
#include "mhcontrol.h"
#include "mhrouter.h"

#define RIG_IC7300 0x88
#define RIG_K3     0x62

static struct ev_loop *loop;
static int failures;

/*
 * Stubs
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

char *w_strdup(const char *src) {
	char *p = strdup(src);
	if(!p)
		abort();
	return p;
}

struct ev_loop *app_ctx_get_loop(app_ctx *ctx) {
	(void)ctx;
	return loop;
}

eventbus_t *app_ctx_get_eventbus(app_ctx *ctx) {
	(void)ctx;
	return NULL;
}

static event_cb_fn keyer_state_cb;
static void *keyer_state_data;
static int mon_lines;
static char mon_last[256];

void eventbus_publish(eventbus_t *bus, enum app_event_type type, const void *data) {
	(void)bus;
	if(type == EV_CAT_MONITOR) {
		const struct ev_cat_monitor *ev = data;
		mon_lines++;
		snprintf(mon_last, sizeof(mon_last), "%s", ev->line);
	}
}

const char *mhc_state_str(int state) {
	return state == MHC_KEYER_STATE_ONLINE ? "ONLINE" : "OFFLINE";
}

eventbus_sub_t *eventbus_subscribe(eventbus_t *bus, enum app_event_type type, event_cb_fn cb, void *user_data) {
	(void)bus;
	if(type == EV_KEYER_STATE) {
		keyer_state_cb = cb;
		keyer_state_data = user_data;
	}
	return (eventbus_sub_t *)&keyer_state_cb;
}

void eventbus_unsubscribe(eventbus_sub_t *sub) {
	(void)sub;
	keyer_state_cb = NULL;
}

static MHRConsumerCallback radio_cb, host_cb;
static void *radio_data, *host_data;
static int radio_channel, host_channel;

void mhr_add_consumer_cb(struct mh_router *router, MHRConsumerCallback cb, int channel, void *user_data) {
	(void)router;
	radio_cb = cb;
	radio_data = user_data;
	radio_channel = channel;
}

void mhr_rem_consumer_cb(struct mh_router *router, MHRConsumerCallback cb, int channel) {
	(void)router;
	if(cb == radio_cb && channel == radio_channel)
		radio_cb = NULL;
}

void mhr_add_producer_tap_cb(struct mh_router *router, MHRConsumerCallback cb, int channel, void *user_data) {
	(void)router;
	host_cb = cb;
	host_data = user_data;
	host_channel = channel;
}

void mhr_rem_producer_tap_cb(struct mh_router *router, MHRConsumerCallback cb, int channel, void *user_data) {
	(void)router;
	if(cb == host_cb && channel == host_channel && user_data == host_data)
		host_cb = NULL;
}

static struct mhc_speed_cfg speed;
static int speed_valid;

int mhc_get_speed_params(struct mh_control *ctl, int channel, struct mhc_speed_cfg *out) {
	(void)ctl; (void)channel;
	if(!speed_valid)
		return -1;
	*out = speed;
	return 0;
}

#define MAX_PUB 64
static struct mhc_radio_info pub[MAX_PUB];
static int npub;

void mhc_update_radio_info(struct mh_control *ctl, const char *source, const struct mhc_radio_info *info) {
	(void)ctl; (void)source;
	if(npub < MAX_PUB)
		pub[npub++] = *info;
}

/*
 * Helpers
 */

#define CHECK(cond, ...) do { \
	if(!(cond)) { \
		printf("FAIL %s:%d: ", __FILE__, __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
		failures++; \
	} \
} while(0)

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

static size_t parse_hex(const char *s, uint8_t *buf) {
	size_t n = 0;
	unsigned v;
	int off;
	while(sscanf(s, " %2x%n", &v, &off) == 1) {
		buf[n++] = v;
		s += off;
	}
	return n;
}

// The router delivers radio data one byte per call.
static void radio(const char *hex) {
	uint8_t buf[64];
	size_t n = parse_hex(hex, buf);
	ev_now_update(loop);
	for(size_t i = 0; i < n && radio_cb; i++)
		radio_cb(NULL, &buf[i], 1, radio_channel, radio_data);
}

// Client data arrives as read from the connector.
static void host(const char *hex) {
	uint8_t buf[64];
	size_t n = parse_hex(hex, buf);
	ev_now_update(loop);
	if(host_cb)
		host_cb(NULL, buf, n, host_channel, host_data);
}

// Command as Hamlib sends it: host data, then the bus echo.
static void cmd(const char *hex) {
	host(hex);
	radio(hex);
}

static void keyer_state(int state) {
	struct ev_keyer_state ev = { .serial = "TEST0001", .state = state };
	if(keyer_state_cb)
		keyer_state_cb(EV_KEYER_STATE, &ev, keyer_state_data);
}

static const struct mhc_radio_info *last_pub(void) {
	return npub ? &pub[npub - 1] : NULL;
}

/*
 * Tests
 */

static int file_contains(const char *path, const char *needle) {
	char line[512];
	int n = 0;
	FILE *f = fopen(path, "r");
	if(!f)
		return -1;
	while(fgets(line, sizeof(line), f))
		if(strstr(line, needle))
			n++;
	fclose(f);
	return n;
}

int main(int argc, char **argv) {
	struct device dev = {
		.router = (struct mh_router *)&dev,	// only compared, never dereferenced
		.ctl = (struct mh_control *)&dev,
		.serial = "TEST0001",
	};
	struct cat_native_cfg cfg = { .radio = 1, .protocol = CAT_NATIVE_PROTO_AUTO };
	const struct mhc_radio_info *p;

	loop = ev_default_loop(0);

	// Channel not configured yet: no decoder, nothing published.
	struct cat_native *cn = cat_native_create((app_ctx *)&dev, &dev, &cfg);
	CHECK(cn && radio_cb && host_cb && keyer_state_cb, "create registers callbacks");
	CHECK(radio_channel == 4 && host_channel == 4, "radio 1 uses channel R1");
	cmd("fe fe 94 e0 25 00 fd");
	radio("fe fe e0 94 25 00 00 40 07 14 00 fd");
	run_for(0.3);
	CHECK(npub == 0, "no decoder before the channel is configured, got %d", npub);

	// Channel configured; the configuration is rechecked at most once per second.
	speed.rigtype = RIG_IC7300;
	speed.icomaddress = 0x94;
	speed_valid = 1;
	run_for(1.05);

	cmd("fe fe 94 e0 25 00 fd");
	radio("fe fe e0 94 25 00 00 40 07 14 00 fd");
	cmd("fe fe 94 e0 26 00 fd");
	radio("fe fe e0 94 26 00 03 00 01 fd");
	run_for(0.1);
	CHECK(npub == 0, "published only after the delay, got %d", npub);
	run_for(0.2);
	p = last_pub();
	CHECK(npub == 1 && p && p->radio == 1 && p->rxFreq == 14074000 && p->txFreq == 14074000 &&
	      p->mode == MOD_CW, "first publication: 14074000 CW (n=%d)", npub);

	// Continuous traffic: published every delay period, not starved.
	npub = 0;
	for(int i = 0; i < 10; i++) {
		cmd("fe fe 94 e0 25 00 fd");
		radio("fe fe e0 94 25 00 00 40 07 14 00 fd");
		run_for(0.05);
	}
	CHECK(npub >= 2, "publishes during continuous polling, got %d", npub);

	// Hamlib set_mode PKTUSB from CW: 06 01 01, then 1A 06 01 01. No VOICE in between.
	npub = 0;
	cmd("fe fe 94 e0 06 01 01 fd");
	radio("fe fe e0 94 fb fd");
	run_for(0.05);
	cmd("fe fe 94 e0 1a 06 01 01 fd");
	radio("fe fe e0 94 fb fd");
	run_for(0.3);
	p = last_pub();
	CHECK(p && p->mode == MOD_DIGITAL, "PKTUSB -> DIGITAL, got %d", p ? p->mode : -9);
	for(int i = 0; i < npub; i++)
		CHECK(pub[i].mode != MOD_VOICE, "no VOICE while switching to PKTUSB");

	// Data flag arrives late: the keyer mode is kept meanwhile.
	cmd("fe fe 94 e0 06 03 01 fd");	// CW
	radio("fe fe e0 94 fb fd");
	run_for(0.3);
	npub = 0;
	cmd("fe fe 94 e0 06 01 01 fd");	// USB, data flag unknown
	radio("fe fe e0 94 fb fd");
	run_for(0.3);
	p = last_pub();
	CHECK(p && p->mode == -1, "keyer mode kept while the data flag is unknown, got %d", p ? p->mode : -9);
	cmd("fe fe 94 e0 1a 06 fd");
	radio("fe fe e0 94 1a 06 01 01 fd");
	run_for(0.3);
	p = last_pub();
	CHECK(p && p->mode == MOD_DIGITAL, "late data flag -> DIGITAL, got %d", p ? p->mode : -9);

	// Data flag never queried (application polls 04 only): VOICE after the wait.
	cmd("fe fe 94 e0 06 03 01 fd");
	radio("fe fe e0 94 fb fd");
	run_for(0.3);
	npub = 0;
	cmd("fe fe 94 e0 04 fd");
	radio("fe fe e0 94 04 00 01 fd");	// LSB
	run_for(0.5);
	p = last_pub();
	CHECK(p && p->mode == -1, "LSB, data flag unknown: mode kept, got %d", p ? p->mode : -9);
	run_for(0.8);
	p = last_pub();
	CHECK(p && p->mode == MOD_VOICE, "LSB after the wait: VOICE, got %d", p ? p->mode : -9);

	// Split: TX on the unselected VFO.
	cmd("fe fe 94 e0 25 01 fd");
	radio("fe fe e0 94 25 01 00 00 05 14 00 fd");
	cmd("fe fe 94 e0 0f fd");
	radio("fe fe e0 94 0f 01 fd");
	run_for(0.3);
	p = last_pub();
	CHECK(p && p->rxFreq == 14074000 && p->txFreq == 14050000 && p->vfoBFreq == 14050000,
	      "split: rx 14074000 tx 14050000");

	// No more traffic: the last state is sent again, so mhcontrol's radio info doesn't expire.
	struct mhc_radio_info before = *p;
	npub = 0;
	run_for(1.2);
	p = last_pub();
	CHECK(npub >= 2 && !memcmp(p, &before, sizeof(before)), "keepalive repeats the last state, got %d", npub);

	// Keyer offline: decoded state is dropped, keepalive stops.
	keyer_state(MHC_KEYER_STATE_OFFLINE);
	keyer_state(MHC_KEYER_STATE_ONLINE);
	npub = 0;
	run_for(0.7);
	CHECK(npub == 0, "no keepalive after the keyer went offline, got %d", npub);
	cmd("fe fe 94 e0 26 00 fd");
	radio("fe fe e0 94 26 00 03 00 01 fd");
	run_for(0.3);
	p = last_pub();
	CHECK(p && p->mode == MOD_CW && p->rxFreq == 0, "state reset after offline (rx=%u)", p ? p->rxFreq : 0);

	// Rig type changed to a radio without a native decoder.
	speed.rigtype = RIG_K3;
	run_for(1.05);
	npub = 0;
	cmd("fe fe 94 e0 25 00 fd");	// first data after the change sets the decoder up
	radio("fe fe e0 94 25 00 00 40 07 14 00 fd");
	run_for(0.3);
	CHECK(npub == 0, "no decoder for a K3, got %d publications", npub);

	// protocol "civ" overrides the rig type, civ_address the channel's address.
	cat_native_destroy(cn);
	CHECK(!radio_cb && !host_cb && !keyer_state_cb, "destroy removes all callbacks");
	cfg.protocol = CAT_NATIVE_PROTO_CIV;
	cfg.civ_address = 0x98;
	cfg.radio = 2;
	cn = cat_native_create((app_ctx *)&dev, &dev, &cfg);
	CHECK(radio_channel == 5 && host_channel == 5, "radio 2 uses channel R2");
	npub = 0;
	cmd("fe fe 98 e0 03 fd");
	radio("fe fe e0 98 03 00 00 08 14 00 fd");
	run_for(0.3);
	p = last_pub();
	CHECK(p && p->radio == 2 && p->rxFreq == 14080000, "protocol civ, address 98 on a K3 channel");
	CHECK(mon_lines == 0, "no monitor output unless enabled, got %d", mon_lines);
	cat_native_destroy(cn);

	// CAT monitor: trace lines to the event bus and to <dir>/cat-<serial>-r1.trace.
	if(argc > 1) {
		char path[512];
		snprintf(path, sizeof(path), "%s/cat-TEST0001-r1.trace", argv[1]);
		cfg = (struct cat_native_cfg){ .radio = 1, .monitor = 1, .monitor_dir = argv[1] };
		speed.rigtype = RIG_IC7300;
		cn = cat_native_create((app_ctx *)&dev, &dev, &cfg);
		cmd("fe fe 94 e0 25 00 fd");
		radio("fe fe e0 94 25 00 00 40 07 14 00 fd");
		cmd("fe fe 94 e0 26 00 fd");
		radio("fe fe e0 94 26 00 01 01 01 fd");
		radio("fc");
		keyer_state(MHC_KEYER_STATE_OFFLINE);
		keyer_state(MHC_KEYER_STATE_ONLINE);
		radio("fe fe 00 94 00 00 00 08 14 00 fd");
		run_for(0.3);
		CHECK(strstr(mon_last, " R fe fe 00 94 00 00 00 08 14 00 fd  # unsolicited, applied: rx=14080000"),
		      "last monitor line: '%s'", mon_last);
		cat_native_destroy(cn);
		CHECK(mon_lines >= 12, "monitor lines published: %d", mon_lines);
		CHECK(file_contains(path, "@civ addr=94") == 2, "config line at start and after the reset");
		CHECK(file_contains(path, " H1 fe fe 94 e0 25 00 fd  # cmd") == 1, "host data tagged H1");
		CHECK(file_contains(path, " R fe fe 94 e0 25 00 fd  # echo") == 1, "echo classified");
		CHECK(file_contains(path, "# reply, applied: rx=14074000 unsel=? mode=PKTUSB") == 1, "reply with state");
		CHECK(file_contains(path, " R fc  # jam") == 1, "collision");
		CHECK(file_contains(path, "# keyer OFFLINE, decoder starts over") == 1, "keyer state comment");
		CHECK(file_contains(path, "# monitor stopped") == 1, "closing comment");
	}

	printf("%s native_test\n", failures ? "FAIL" : "ok  ");
	return failures ? 1 : 0;
}
