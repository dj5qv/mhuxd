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
 * state, CAT monitor, own queries. Built with a keepalive of 0.5 s and values
 * stale after 0.6 s (see Makefile).
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
#include "cat_ascii.h"
#include "app_ctx.h"
#include "device.h"
#include "eventbus.h"
#include "events.h"
#include "mhcontrol.h"
#include "mhrouter.h"
#include "stubs.h"

/*
 * Router stubs
 */

#define CLIENT_FD 42	// the connector the test client is on

static MHRConsumerCallback radio_cb;
static MHRTapCallback host_cb;
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

void mhr_add_producer_tap_cb(struct mh_router *router, MHRTapCallback cb, int channel, void *user_data) {
	(void)router;
	host_cb = cb;
	host_data = user_data;
	host_channel = channel;
}

void mhr_rem_producer_tap_cb(struct mh_router *router, MHRTapCallback cb, int channel, void *user_data) {
	(void)router;
	if(cb == host_cb && channel == host_channel && user_data == host_data)
		host_cb = NULL;
}

// Own queries: what mhuxd sent, whether client data is held and radio data intercepted, and
// what the clients got from the radio.
static uint8_t sent[64];
static size_t nsent;
static int nsends;
static int held, intercepted;
static uint8_t client_rx[1024];
static size_t nclient_rx;
static uint8_t held_host[256];
static size_t nheld_host;
static ev_timer release_timer;

int mhr_send_in(struct mh_router *router, const uint8_t *data, unsigned int len, int channel) {
	(void)router; (void)channel;
	if(len > sizeof(sent))
		return -1;
	memcpy(sent, data, len);
	nsent = len;
	nsends++;
	return len;
}

static void to_client(const uint8_t *data, size_t len) {
	if(nclient_rx + len <= sizeof(client_rx)) {
		memcpy(client_rx + nclient_rx, data, len);
		nclient_rx += len;
	}
}

void mhr_pass_out(struct mh_router *router, const uint8_t *data, unsigned int len, int channel, int fd) {
	(void)router; (void)channel;
	CHECK(fd == MHR_ALL || fd == MHR_UNSOLICITED || fd == CLIENT_FD, "radio data passed to connector %d", fd);
	to_client(data, len);
}

// The client takes transceive and auto information.
int mhr_count_consumers_flagged(struct mh_router *router, int channel, unsigned flags) {
	(void)router; (void)channel; (void)flags;
	return 0;
}

// A single connector: no turns.
void mhr_set_producer_turn(struct mh_router *router, int channel, int fd) {
	(void)router; (void)channel; (void)fd;
}

int mhr_next_waiting_producer(struct mh_router *router, int channel, int fd) {
	(void)router; (void)channel; (void)fd;
	return -1;
}

int mhr_count_producers(struct mh_router *router, int channel) {
	(void)router; (void)channel;
	return 1;
}

const char *mhr_get_producer_tag(struct mh_router *router, int channel, int fd) {
	(void)router; (void)channel;
	return fd == CLIENT_FD ? "cat1" : NULL;
}

void mhr_intercept_consumers(struct mh_router *router, int channel, int intercept) {
	(void)router; (void)channel;
	intercepted = intercept;
}

// Released client data is read on the next loop iteration, as in the router.
static void release_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	(void)l; (void)w; (void)revents;
	if(held || !nheld_host)
		return;
	size_t n = nheld_host;
	nheld_host = 0;
	if(host_cb)
		host_cb(NULL, held_host, n, host_channel, CLIENT_FD, host_data);
}

void mhr_hold_producers(struct mh_router *router, int channel, int hold) {
	(void)router; (void)channel;
	held = hold;
	if(!hold && nheld_host) {
		ev_timer_set(&release_timer, 0., 0.);
		ev_timer_start(loop, &release_timer);
	}
}

/*
 * Helpers
 */

// The router delivers radio data one byte per call, to the clients unless intercepted.
static void radio_bytes(const uint8_t *buf, size_t n) {
	ev_now_update(loop);
	for(size_t i = 0; i < n && radio_cb; i++) {
		if(!intercepted)
			to_client(&buf[i], 1);
		radio_cb(NULL, &buf[i], 1, radio_channel, radio_data);
	}
}

static void radio(const char *hex) {
	uint8_t buf[64];
	size_t n = parse_hex(hex, buf);
	radio_bytes(buf, n);
}

// Client data arrives as read from the connector; while held it waits.
static void host_bytes(const uint8_t *buf, size_t n) {
	ev_now_update(loop);
	if(held) {
		memcpy(held_host + nheld_host, buf, n);
		nheld_host += n;
	} else if(host_cb) {
		host_cb(NULL, buf, n, host_channel, CLIENT_FD, host_data);
	}
}

static void host(const char *hex) {
	uint8_t buf[64];
	size_t n = parse_hex(hex, buf);
	host_bytes(buf, n);
}

// ASCII protocols: the radio's answer, as text.
static void radio_text(const char *text) {
	radio_bytes((const uint8_t *)text, strlen(text));
}

static void host_text(const char *text) {
	host_bytes((const uint8_t *)text, strlen(text));
}

// Command as Hamlib sends it: host data, then the bus echo.
static void cmd(const char *hex) {
	host(hex);
	radio(hex);
}


// Runs the loop until mhuxd sent an own query, at most s seconds. Returns the time it took,
// or -1.
static double wait_query(double s) {
	int before = nsends;
	double waited = 0;
	while(nsends == before && waited < s) {
		run_for(0.02);
		waited += 0.02;
	}
	return nsends != before ? waited : -1;
}

static int sent_is(const char *hex) {
	uint8_t buf[64];
	size_t n = parse_hex(hex, buf);
	return n == nsent && !memcmp(buf, sent, n);
}

static int sent_text_is(const char *text) {
	return strlen(text) == nsent && !memcmp(text, sent, nsent);
}

static int client_got(const char *hex) {
	uint8_t buf[64];
	size_t n = parse_hex(hex, buf);
	return n == nclient_rx && !memcmp(buf, client_rx, n);
}

// Plays the IC-7300 for mhuxd's own CI-V queries: echo, then the answer. With nak_26 the radio
// doesn't know command 26.
static void civ_answer(int nak_26) {
	uint8_t q[64];
	size_t n = nsent;

	memcpy(q, sent, n);
	radio_bytes(q, n);
	switch(q[4]) {
	case 0x03: radio("fe fe e5 94 03 00 00 07 14 00 fd"); break;
	case 0x04: radio("fe fe e5 94 04 01 01 fd"); break;
	case 0x1a: radio("fe fe e5 94 1a 06 01 01 fd"); break;
	case 0x26: radio(nak_26 ? "fe fe e5 94 fa fd" : "fe fe e5 94 26 00 01 01 01 fd"); break;
	}
}

static void test_own_queries(struct device *dev) {
	struct cat_native_cfg cfg = { .radio = 1, .own_queries = 1 };
	const struct mhc_radio_info *p;
	struct cat_native *cn;
	double t;

	speed.rigtype = RIG_IC7300;
	speed.icomaddress = 0x94;
	nsends = 0;
	nclient_rx = 0;
	npub = 0;
	cn = cat_native_create((app_ctx *)dev, dev, &cfg);

	// Nothing known, line idle: frequency first. Client data waits, radio data is intercepted.
	t = wait_query(0.5);
	CHECK(t >= 0 && sent_is("fe fe 94 e5 03 fd"), "own frequency query (%.2f s)", t);
	CHECK(held && intercepted, "client data held, radio data intercepted");
	host("fe fe 94 e0 04 fd");
	CHECK(nheld_host == 6, "client command waits");
	radio("fe fe 94 e5 03 fd");
	CHECK(held, "still held after the echo");
	radio("fe fe e5 94 03 00 40 07 14 00 fd");
	CHECK(!held && !intercepted, "released after the answer");
	CHECK(nclient_rx == 0, "echo and answer not passed to the client, got %zu bytes", nclient_rx);
	run_for(0.01);
	CHECK(nheld_host == 0, "held client command sent after the answer");
	radio("fe fe 94 e0 04 fd");
	radio("fe fe e0 94 04 01 01 fd");
	CHECK(client_got("fe fe 94 e0 04 fd fe fe e0 94 04 01 01 fd"), "client gets its echo and reply");

	// USB, data flag unknown: 26 00 as soon as the client's command is answered, the line is
	// free until its next one. A transceive frame between echo and answer goes to the client.
	CHECK(nsends == 2 && sent_is("fe fe 94 e5 26 00 fd") && held && intercepted,
	      "own mode query right after the client's answer");
	nclient_rx = 0;
	radio("fe fe 94 e5 26 00 fd");
	radio("fe fe 00 94 00 00 00 08 14 00 fd");
	radio("fe fe e5 94 26 00 01 01 01 fd");
	CHECK(client_got("fe fe 00 94 00 00 00 08 14 00 fd"), "transceive passed on, own frames not");
	CHECK(!held && !intercepted, "released after the mode answer");
	run_for(0.25);
	p = last_pub();
	CHECK(p && p->mode == MOD_DIGITAL && p->rxFreq == 14080000, "own answers published: 14080000 PKTUSB");
	CHECK(wait_query(0.25) < 0, "no own query while the values are fresh");

	// A client polling keeps the values fresh: no own queries.
	nsends = 0;
	for(int i = 0; i < 5; i++) {
		cmd("fe fe 94 e0 03 fd");
		radio("fe fe e0 94 03 00 00 08 14 00 fd");
		cmd("fe fe 94 e0 26 00 fd");
		radio("fe fe e0 94 26 00 01 01 01 fd");
		run_for(0.3);
	}
	CHECK(nsends == 0, "no own queries while a client polls, got %d", nsends);

	// No answer: client data released after the timeout, a late answer is still taken out.
	nclient_rx = 0;
	t = wait_query(2.0);
	CHECK(t >= 0 && sent_is("fe fe 94 e5 03 fd"), "own frequency query once stale (%.2f s)", t);
	run_for(0.35);
	CHECK(!held && intercepted, "timeout: client data released, radio data still routed");
	radio("fe fe 94 e5 03 fd");
	radio("fe fe e5 94 03 00 00 07 14 00 fd");
	CHECK(nclient_rx == 0 && !intercepted, "late answer taken out, routing ends (%zu bytes)", nclient_rx);

	// The radio doesn't know 26: after three rejections 04 and 1A 06 instead.
	int n26 = 0, n04 = 0, n1a = 0;
	for(int i = 0; i < 12 && !(n04 && n1a); i++) {
		if(wait_query(2.0) < 0)
			break;
		n26 += sent[4] == 0x26;
		n04 += sent[4] == 0x04;
		n1a += sent[4] == 0x1a;
		civ_answer(1);
	}
	CHECK(n26 == 3 && n04 && n1a, "26 rejected three times, then 04 and 1A 06 (%d %d %d)", n26, n04, n1a);
	CHECK(nclient_rx == 0, "client got nothing of it, %zu bytes", nclient_rx);

	// Keyer offline: no queries.
	keyer_state_val = MHC_KEYER_STATE_OFFLINE;
	nsends = 0;
	run_for(1.0);
	CHECK(nsends == 0, "no own queries while the keyer is offline, got %d", nsends);
	keyer_state_val = MHC_KEYER_STATE_ONLINE;

	// Radio off (silent since the keyer went offline): queries time out, the keepalive stops,
	// so the radio state expires in mhcontrol.
	run_for(0.6);
	int n = npub;
	run_for(1.2);
	CHECK(npub == n, "no keepalive while the radio is silent (%d -> %d)", n, npub);
	CHECK(!held, "nothing held while the radio is silent");

	cat_native_destroy(cn);
	CHECK(!held && !intercepted, "destroy releases client and radio data");

	// Kenwood TS-590: IF, then DA for the data flag of USB.
	speed.rigtype = RIG_TS590;
	nclient_rx = 0;
	npub = 0;
	cn = cat_native_create((app_ctx *)dev, dev, &cfg);
	t = wait_query(0.5);
	CHECK(t >= 0 && sent_text_is("IF;"), "TS-590: own IF query");
	radio_text("IF00014074000     +000000000021000000;");
	t = wait_query(0.5);
	CHECK(t >= 0 && sent_text_is("DA;"), "TS-590: DA after IF");
	radio_text("DA1;");
	run_for(0.25);
	p = last_pub();
	CHECK(p && p->rxFreq == 14074000 && p->mode == MOD_DIGITAL, "TS-590: 14074000 PKTUSB from own queries");
	CHECK(nclient_rx == 0 && !held && !intercepted, "TS-590: answers taken out, %zu bytes", nclient_rx);

	// A client set has no answer: it blocks own queries for CAT_ASCII_SET_WAIT_MS only.
	host_text("FA00014080000;");
	nsends = 0;
	t = wait_query(CAT_ASCII_SET_WAIT_MS / 1000.0 - 0.02);
	CHECK(t < 0, "no own query right after a client set");
	t = wait_query(1.5);
	CHECK(t >= 0 && sent_text_is("IF;"), "own queries again after a client set");
	cat_native_destroy(cn);
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
	ev_timer_init(&release_timer, release_cb, 0., 0.);

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

	// Kenwood TS-590: Kenwood-style ASCII, data mode via DA.
	cfg = (struct cat_native_cfg){ .radio = 1 };
	speed.rigtype = RIG_TS590;
	cn = cat_native_create((app_ctx *)&dev, &dev, &cfg);
	npub = 0;
	radio_text("FA00014074000;MD2;DA1;");
	run_for(0.3);
	p = last_pub();
	CHECK(p && p->rxFreq == 14074000 && p->mode == MOD_DIGITAL && !strcmp(p->mode_str, "PKTUSB"),
	      "TS-590: 14074000 PKTUSB -> DIGITAL (mode_str '%s')", p ? p->mode_str : "");
	cat_native_destroy(cn);

	// Legacy Yaesu (binary protocol): no decoder.
	speed.rigtype = RIG_FT817;
	cn = cat_native_create((app_ctx *)&dev, &dev, &cfg);
	npub = 0;
	radio_text("FA00014074000;MD2;");
	run_for(0.3);
	CHECK(npub == 0, "no decoder for the FT-817, got %d publications", npub);
	cat_native_destroy(cn);

	CHECK(nsends == 0, "nothing sent to the radio without own queries, got %d", nsends);
	test_own_queries(&dev);

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
		CHECK(file_contains(path, " R fe fe 94 e0 25 00 fd  # H1 echo") == 1, "echo classified");
		CHECK(file_contains(path, "# H1 reply, applied: rx=14074000 unsel=? mode=PKTUSB") == 1, "reply with state");
		CHECK(file_contains(path, "# H1: connector cat1") == 1, "client named");
		CHECK(file_contains(path, " R fc  # jam") == 1, "collision");
		CHECK(file_contains(path, "# keyer OFFLINE, decoder starts over") == 1, "keyer state comment");
		CHECK(file_contains(path, "# monitor stopped") == 1, "closing comment");
	}

	printf("%s native_test\n", failures ? "FAIL" : "ok  ");
	return failures ? 1 : 0;
}
