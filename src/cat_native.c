/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <errno.h>
#include <ev.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cat_native.h"
#include "cat_ascii.h"
#include "cat_civ.h"
#include "cat_state.h"
#include "cat_trace.h"
#include "cat_ybin.h"
#include "app_ctx.h"
#include "channel.h"
#include "conmgr.h"
#include "device.h"
#include "eventbus.h"
#include "events.h"
#include "mhcontrol.h"
#include "mhrouter.h"
#include "radiotypes.h"
#include "util.h"
#include "logger.h"

#define MOD_ID "catn"
#define CATN_SOURCE "native"	// rig mode sync backend name, shown in the API

// Decoded frames of one burst (query/reply pairs, set + FB) are published together.
#define CATN_PUBLISH_DELAY 0.2
// Hamlib sets USB-D as 06 (USB) followed by 1A 06 (data on). While the data flag is unknown,
// the keyer mode is kept for this long instead of switching to VOICE and back.
#define CATN_DATA_WAIT 1.0
// The channel's rig type and CI-V address are rechecked at most this often.
#define CATN_CFG_CHECK_IVAL 1.0
// The last decoded state is sent again this often, below mhcontrol's RADIO_INFO_TIMEOUT (10 s).
// Applications that stop polling don't make the radio state unknown; without it the keyer
// would fall back to its forced mode (r1ForceKeyerMode) whenever no application runs.
#ifndef CATN_KEEPALIVE
#define CATN_KEEPALIVE 5.0
#endif

// CAT monitor files (LOGDIR/cat-<serial>-r<n>.trace) stop growing at this size.
#define CATN_MON_MAX_BYTES (20 * 1024 * 1024)

// Own queries: when no application keeps the radio state fresh, mhuxd asks the radio itself,
// one query at a time: right after an application's command was answered, or when the line has
// been quiet for a moment. The applications' data waits in the router meanwhile; radio frames
// answering the query are taken out of the radio data.
#define CATN_TICK 0.1		// the poll condition (and the number of connectors) is checked this often
#define CATN_OWN_IDLE 0.15	// no CAT data in either direction for this long before a query,
				// unless an application's command was just answered
#ifndef CATN_OWN_STALE
#define CATN_OWN_STALE 1.5	// values older than this are queried, each query at most this often
#endif
#define CATN_OWN_TIMEOUT 0.3	// answer expected within; the applications' data waits meanwhile
#define CATN_OWN_BYTE_TIME 0.0025	// plus this per byte of a known answer length
#ifndef CATN_OWN_RETRY
#define CATN_OWN_RETRY 10.0	// query interval while the radio doesn't answer
#endif
#define CATN_OWN_MAX_TIMEOUTS 3
#define CATN_OWN_MAX_REJECTS 3	// rejected this often in a row: the radio doesn't know the query
// With own queries, the keepalive stops when nothing came from the radio for this long (radio
// off), so mhcontrol falls back to the keyer's configured mode as it does when rigctld loses
// the radio.
#define CATN_RADIO_SILENT (2 * CATN_KEEPALIVE)
#define CATN_CIV_OWN_ADDR 0xe5	// mhuxd's CI-V controller address, applications use E0
#define CATN_ROUTE_MAX 256	// radio data held back until its frame is complete
#define CATN_ROUTE_FLUSH 0.1	// held radio data that doesn't complete a frame is passed on after

// Several applications, each on a connector of its own: one command at a time on the CAT line.
// An application's turn ends when the radio answered its commands; the next application with
// data waiting gets the line. Answers go to the application that asked.
#ifndef CATN_TURN_TIMEOUT
#define CATN_TURN_TIMEOUT 0.5	// no answer this long after the application's last data: given up
#endif
#define CATN_TURN_TICK 0.02	// turn end and timeout checked this often, besides on radio data

// CI-V bus collisions (civ_collision_resend): a command destroyed by a collision with the
// radio's own messages (transceive) is sent again by mhuxd, right after the radio's next frame
// (the bus is free then) or once the radio was quiet for CATN_RESEND_QUIET, longer than the
// 30-50 ms between broadcasts while the VFO knob turns. The cut-off frame and the jam (FC)
// don't reach the applications.
#define CATN_RESEND_QUIET 0.06
#define CATN_RESEND_MAX 3

// Destination of radio data while it is routed: a connector fd, MHR_ALL, MHR_UNSOLICITED, or
// one of these.
#define ROUTE_NONE (-4)		// no frame completed yet
#define ROUTE_DROP (-3)		// answer to an own query

// Rig types (keyer protocol keys, src/radiotypes.c) that need special handling.
#define RIG_TYPE_IC735   0x08	// 4-byte frequencies
#define RIG_TYPE_OMNI_VI 0x38	// Ten-Tec, speaks CI-V
#define RIG_TYPE_TS950   0x19	// data mode via DT, not DA

// Rig types with a Kenwood-style ASCII protocol. Icom radios are recognized by their CI-V
// address, the older Yaesu radios are in ybin_rigs. Ten-Tec, JRC, Hilberling, Perseus, FT-767:
// no decoder yet.
static const struct {
	uint8_t key;
	uint8_t protocol;
	uint8_t no_data;	// Kenwood without data mode (no DA/DT)
} rig_protocols[] = {
	{ 0x4C, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 140
	{ 0x15, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 440
	{ 0x16, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 450
	{ 0x47, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 480
	{ 0x1A, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 570
	{ 0x7D, CAT_NATIVE_PROTO_KENWOOD, 0 },	// TS 590: DA
	{ 0x4D, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 680
	{ 0x17, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 690
	{ 0x4E, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 711
	{ 0x4F, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 790
	{ 0x50, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 811
	{ 0x18, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 850
	{ 0x1B, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 870
	{ 0x51, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 940
	{ 0x19, CAT_NATIVE_PROTO_KENWOOD, 0 },	// TS 950: DT
	{ 0x7E, CAT_NATIVE_PROTO_KENWOOD, 0 },	// TS 990: DA, OM
	{ 0x1C, CAT_NATIVE_PROTO_KENWOOD, 1 },	// TS 2000
	{ 0x44, CAT_NATIVE_PROTO_ELECRAFT, 0 },	// K2
	{ 0x62, CAT_NATIVE_PROTO_ELECRAFT, 0 },	// K3
	{ 0x75, CAT_NATIVE_PROTO_ELECRAFT, 0 },	// K3 (patched)
	{ 0x7A, CAT_NATIVE_PROTO_ELECRAFT, 0 },	// K3 (patched 2)
	{ 0x64, CAT_NATIVE_PROTO_YAESU, 0 },	// FT-450
	{ 0x67, CAT_NATIVE_PROTO_YAESU, 0 },	// FT-950
	{ 0x84, CAT_NATIVE_PROTO_YAESU, 0 },	// FT-991
	{ 0x60, CAT_NATIVE_PROTO_YAESU, 0 },	// FT-2000
	{ 0x72, CAT_NATIVE_PROTO_YAESU, 0 },	// FT-2000D
	{ 0x81, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx1200
	{ 0x7F, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx3000
	{ 0x79, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx5000
	{ 0x58, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx9000D
	{ 0x68, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx9000D (patched)
	{ 0x69, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx9000MP
	{ 0x6A, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx9000MP (patched)
	{ 0x6B, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx9000 Contest
	{ 0x6C, CAT_NATIVE_PROTO_YAESU, 0 },	// FTdx9000 Contest (patched)
	{ 0x8F, CAT_NATIVE_PROTO_FLEX, 0 },	// Apache Labs ANAN 100/200
	{ 0x8C, CAT_NATIVE_PROTO_FLEX, 0 },	// Expert Electronics MB1
	{ 0x8D, CAT_NATIVE_PROTO_FLEX, 0 },	// Flex 5000
	{ 0x8E, CAT_NATIVE_PROTO_FLEX, 0 },	// Flex 6000 series
	{ 0x83, CAT_NATIVE_PROTO_FLEX, 0 },	// TS 2000 compatible SDR
};

// Older Yaesu radios with the binary protocol: the answer lengths depend on the model.
static const struct {
	uint8_t key;
	uint8_t model;
} ybin_rigs[] = {
	{ 0x45, CAT_YBIN_FT757GX2 },	// FT-757GXII
	{ 0x29, CAT_YBIN_FT817 },
	{ 0x3D, CAT_YBIN_FT857 },
	{ 0x2B, CAT_YBIN_FT897 },
	{ 0x2A, CAT_YBIN_FT847 },
	{ 0x43, CAT_YBIN_FT840 },
	{ 0x55, CAT_YBIN_FT890 },
	{ 0x3A, CAT_YBIN_FT900 },
	{ 0x3B, CAT_YBIN_FT920 },
	{ 0x3C, CAT_YBIN_FT990 },
	{ 0x1F, CAT_YBIN_FT1000 },
	{ 0x20, CAT_YBIN_FT1000 },	// FT-1000D
	{ 0x21, CAT_YBIN_FT1000MP },
	{ 0x22, CAT_YBIN_FT1000MP },	// MARK-V
	{ 0x23, CAT_YBIN_FT1000MP },	// MARK-V Field
};

static const char *protocol_names[] = {
	[CAT_NATIVE_PROTO_AUTO] = "auto",
	[CAT_NATIVE_PROTO_CIV] = "civ",
	[CAT_NATIVE_PROTO_KENWOOD] = "kenwood",
	[CAT_NATIVE_PROTO_ELECRAFT] = "elecraft",
	[CAT_NATIVE_PROTO_YAESU] = "yaesu",
	[CAT_NATIVE_PROTO_FLEX] = "flex",
	[CAT_NATIVE_PROTO_YBIN] = "yaesu_bin",
	[CAT_NATIVE_PROTO_NONE] = "none",
};

// Own queries, per value.
enum {
	Q_FREQ,		// CI-V 03; IF on Kenwood, Elecraft, Yaesu (with the mode); FA on Flex;
			// the status data with the mode on older Yaesu radios
	Q_MODE,		// CI-V 26 00 (with the data flag) or 04; MD on Flex
	Q_DATA,		// CI-V 1A 06 when the radio has no 26; DA/DT on Kenwood, DT on Elecraft
	Q_NUM
};

static const char *q_names[Q_NUM] = { "frequency", "mode", "data mode" };

struct cat_native {
	struct ev_loop *loop;
	struct mh_control *ctl;
	struct mh_router *router;
	char *serial;
	uint8_t radio;
	int channel;
	int protocol;
	int civ_address;
	eventbus_t *ebus;

	// CAT monitor: trace lines to a file and to the event bus (WebSocket).
	int monitor;
	char *mon_path;
	FILE *mon_file;
	long mon_bytes;

	// Decoder, set up from the channel configuration.
	int rigtype;
	int civ_addr;
	struct cat_civ *civ;		// one of these, or none
	struct cat_ascii *ascii;
	struct cat_ybin *ybin;
	int decoder_protocol;		// CAT_NATIVE_PROTO_* of the decoder
	int no_data;			// Kenwood without data mode
	struct cat_state st;
	ev_tstamp cfg_checked;
	int decoder_warned;

	ev_timer publish_timer;
	ev_timer keepalive_timer;
	ev_tstamp data_unknown_since;	// 0 = data flag known or irrelevant
	struct mhc_radio_info last_info;
	int last_info_valid;
	eventbus_sub_t *kscb;

	// Own queries
	int own_queries;		// configured
	ev_timer tick;
	ev_timer own_timer;		// waiting for the answer
	int own_active;			// query sent, applications' data held
	int own_slot;			// Q_* of the query in flight
	ev_tstamp own_timeout;		// for the query in flight
	ev_tstamp own_sent[Q_NUM];
	ev_tstamp own_last;		// last query
	uint8_t own_rejects[Q_NUM];
	uint8_t own_unsupported[Q_NUM];
	uint8_t civ_mode_cmd;		// 0x26, or 0x04 if the radio doesn't know 26
	ev_tstamp own_cat_on;		// FT-847: CAT ON sent, 0 = not yet
	int own_timeouts;		// in a row
	int client_answered;		// the current radio byte completed an answer to an application's query
	ev_tstamp last_host, last_radio;	// last CAT data
	int radio_silent;		// keepalive stopped, see CATN_RADIO_SILENT

	// Radio data routing while an own query can be answered, or while several applications
	// share the radio: data is held until its frame is complete; frames answering an own query
	// are dropped, answers to an application go to it, the rest to all.
	int routing;
	uint8_t route_buf[CATN_ROUTE_MAX];
	size_t route_len;
	int route_dest;			// for the frames completed by the current byte, ROUTE_NONE if none
	ev_timer route_timer;		// CATN_ROUTE_FLUSH

	// Applications: client n (trace tag Hn) is the connector clients_fd[n - 1], numbered when
	// it first sends data.
	struct app_ctx *ctx;
	int clients_fd[CAT_MAX_CLIENTS];	// -1: free
	int connectors;			// on the channel, last count
	int multi;			// more than one, with a decoder: taking turns
	int civ_resend;			// configured, see CATN_RESEND_QUIET
	ev_timer resend_timer;
	int radio_frame_done;		// the current radio byte completed a frame other than a jam
	int no_unsolicited;		// connectors that don't take transceive / AI, last count
	int filter;			// some, with a decoder: holding transceive / AI back from them
	int filter_warned;		// some, but no decoder for the rig type
	int turn_fd;			// connector whose commands are on the line, -1: none
	int turn_client;
	int turn_fresh;			// granted, its data not read yet
	ev_tstamp turn_t;		// granted, or its last data
	ev_timer turn_timer;
};

static const char *radio_str(uint8_t radio) {
	return radio == 2 ? "r2" : "r1";
}

// The older Yaesu radios' decoder needs the model: auto only, from the rig type.
int cat_native_protocol_from_str(const char *s) {
	if(!s)
		return CAT_NATIVE_PROTO_AUTO;
	for(int i = CAT_NATIVE_PROTO_AUTO; i <= CAT_NATIVE_PROTO_FLEX; i++)
		if(!strcmp(s, protocol_names[i]))
			return i;
	return -1;
}

const char *cat_native_protocol_str(int protocol) {
	if((unsigned)protocol > CAT_NATIVE_PROTO_NONE)
		return "?";
	return protocol_names[protocol];
}

static int protocol_of_rig(int rigtype, int *no_data, int *ybin_model) {
	*no_data = 0;
	*ybin_model = -1;
	if(rtyp_get_icom_address(rigtype) != 0 || rigtype == RIG_TYPE_OMNI_VI)
		return CAT_NATIVE_PROTO_CIV;
	for(size_t i = 0; i < sizeof(ybin_rigs) / sizeof(ybin_rigs[0]); i++) {
		if(ybin_rigs[i].key == rigtype) {
			*ybin_model = ybin_rigs[i].model;
			return CAT_NATIVE_PROTO_YBIN;
		}
	}
	for(size_t i = 0; i < sizeof(rig_protocols) / sizeof(rig_protocols[0]); i++) {
		if(rig_protocols[i].key == rigtype) {
			*no_data = rig_protocols[i].no_data;
			return rig_protocols[i].protocol;
		}
	}
	return CAT_NATIVE_PROTO_NONE;
}

static int catn_has_decoder(const struct cat_native *cn) {
	return cn->civ || cn->ascii || cn->ybin;
}

static void catn_destroy_decoder(struct cat_native *cn) {
	cat_civ_destroy(cn->civ);
	cat_ascii_destroy(cn->ascii);
	cat_ybin_destroy(cn->ybin);
	cn->civ = NULL;
	cn->ascii = NULL;
	cn->ybin = NULL;
}

static const struct cat_stats *catn_stats(const struct cat_native *cn) {
	if(cn->civ)
		return cat_civ_get_stats(cn->civ);
	if(cn->ascii)
		return cat_ascii_get_stats(cn->ascii);
	if(cn->ybin)
		return cat_ybin_get_stats(cn->ybin);
	return NULL;
}

static unsigned catn_pending(const struct cat_native *cn) {
	uint64_t t_ms = (uint64_t)(ev_now(cn->loop) * 1000.0);
	if(cn->civ)
		return cat_civ_pending(cn->civ, t_ms);
	if(cn->ascii)
		return cat_ascii_pending(cn->ascii, t_ms);
	if(cn->ybin)
		return cat_ybin_pending(cn->ybin, t_ms);
	return 0;
}

static int catn_client_busy(const struct cat_native *cn, int client) {
	if(client < 1)
		return 0;
	uint64_t t_ms = (uint64_t)(ev_now(cn->loop) * 1000.0);
	if(cn->civ)
		return cat_civ_client_busy(cn->civ, client, t_ms);
	if(cn->ascii)
		return cat_ascii_client_busy(cn->ascii, client, t_ms);
	if(cn->ybin)
		return cat_ybin_client_busy(cn->ybin, client, t_ms);
	return 0;
}

// Older Yaesu radios: an answer is coming in for the client (0: mhuxd's own query). It isn't
// timed out, a memory dump takes seconds.
static int catn_answering(const struct cat_native *cn, int client) {
	int c, own;
	if(!cn->ybin || !cat_ybin_answering(cn->ybin, (uint64_t)(ev_now(cn->loop) * 1000.0), &c, &own))
		return 0;
	return client ? !own && c == client : own;
}

static void catn_client_discard(struct cat_native *cn, int client) {
	if(cn->civ)
		cat_civ_client_discard(cn->civ, client);
	else if(cn->ascii)
		cat_ascii_client_discard(cn->ascii, client);
	else if(cn->ybin)
		cat_ybin_client_discard(cn->ybin, client);
}

static void catn_decoder_feed(struct cat_native *cn, enum cat_dir dir, int client, const uint8_t *data, size_t len,
			      uint64_t t_ms) {
	if(cn->civ)
		cat_civ_feed(cn->civ, dir, client, data, len, t_ms);
	else if(cn->ascii)
		cat_ascii_feed(cn->ascii, dir, client, data, len, t_ms);
	else if(cn->ybin)
		cat_ybin_feed(cn->ybin, dir, client, data, len, t_ms);
}

/*
 * CAT monitor
 */

static void catn_mon_line(struct cat_native *cn, const char *line) {
	if(!cn->monitor)
		return;

	if(cn->mon_file) {
		int r = fprintf(cn->mon_file, "%s\n", line);
		if(r > 0)
			cn->mon_bytes += r;
		if(r < 0 || cn->mon_bytes >= CATN_MON_MAX_BYTES) {
			if(r < 0)
				warn("%s %s CAT monitor: write error on %s, file output stopped", cn->serial,
				     radio_str(cn->radio), cn->mon_path);
			else
				warn("%s %s CAT monitor: %s reached %d MB, file output stopped", cn->serial,
				     radio_str(cn->radio), cn->mon_path, CATN_MON_MAX_BYTES / (1024 * 1024));
			fprintf(cn->mon_file, "# file size limit reached, monitor stopped\n");
			fclose(cn->mon_file);
			cn->mon_file = NULL;
		}
	}

	eventbus_publish(cn->ebus, EV_CAT_MONITOR, &(struct ev_cat_monitor) {
		.serial = cn->serial,
		.radio = cn->radio,
		.line = line,
	});
}

// Wall clock time t_ms (ms of the event loop's time), "HH:MM:SS.mmm".
static void catn_mon_time_ms(uint64_t t_ms, char *buf, size_t sz) {
	time_t secs = (time_t)(t_ms / 1000);
	struct tm tm;

	localtime_r(&secs, &tm);
	snprintf(buf, sz, "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(t_ms % 1000));
}

static void catn_mon_time(const struct cat_native *cn, char *buf, size_t sz) {
	catn_mon_time_ms((uint64_t)(ev_now(cn->loop) * 1000.0), buf, sz);
}

// "# HH:MM:SS.mmm <text>": the time inside the comment keeps the file a replayable trace.
static void catn_mon_comment(struct cat_native *cn, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void catn_mon_comment(struct cat_native *cn, const char *fmt, ...) {
	char line[256];
	va_list ap;
	int n;

	if(!cn->monitor)
		return;
	line[0] = '#';
	line[1] = ' ';
	catn_mon_time(cn, line + 2, sizeof(line) - 2);
	n = strlen(line);
	line[n++] = ' ';
	va_start(ap, fmt);
	vsnprintf(line + n, sizeof(line) - n, fmt, ap);
	va_end(ap);
	catn_mon_line(cn, line);
}

// With the frame's own time: the older Yaesu radios' decoder notices a command or answer that
// stops short only later, the trace replays the same way with the time of its last byte.
static void catn_mon_frame(struct cat_native *cn, const struct cat_frame *f) {
	char time_str[32], line[CAT_TRACE_LINE_MAX];

	catn_mon_time_ms(f->t_ms, time_str, sizeof(time_str));
	cat_trace_frame(line, sizeof(line), time_str, f, &cn->st);
	catn_mon_line(cn, line);
}

static void catn_mon_open(struct cat_native *cn, const char *dir) {
	size_t len = strlen(dir) + strlen(cn->serial) + 32;
	char date[64];
	time_t now = time(NULL);
	struct tm tm;

	cn->mon_path = w_malloc(len);
	snprintf(cn->mon_path, len, "%s/cat-%s-%s.trace", dir, cn->serial, radio_str(cn->radio));

	cn->mon_file = fopen(cn->mon_path, "ae");
	if(cn->mon_file && ftell(cn->mon_file) >= CATN_MON_MAX_BYTES) {
		// Too big from earlier sessions: start over.
		fclose(cn->mon_file);
		cn->mon_file = fopen(cn->mon_path, "we");
	}
	if(!cn->mon_file) {
		warn_e(errno, "%s %s CAT monitor: can't open %s, monitor only via WebSocket", cn->serial,
		       radio_str(cn->radio), cn->mon_path);
		return;
	}
	setvbuf(cn->mon_file, NULL, _IOLBF, 0);
	cn->mon_bytes = ftell(cn->mon_file);

	localtime_r(&now, &tm);
	strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S %z", &tm);
	info("%s %s CAT monitor on, writing %s", cn->serial, radio_str(cn->radio), cn->mon_path);
	catn_mon_comment(cn, "mhuxd CAT monitor, %s %s, started %s", cn->serial, radio_str(cn->radio), date);
}

/*
 * Radio data routing
 */

// Radio data that belongs to no client's command: to the application whose turn it is, to all
// when none has it.
static int catn_route_default(const struct cat_native *cn) {
	return cn->turn_fd >= 0 ? cn->turn_fd : MHR_ALL;
}

static void catn_route_flush(struct cat_native *cn, int dest) {
	ev_timer_stop(cn->loop, &cn->route_timer);
	if(cn->route_len && dest != ROUTE_DROP)
		mhr_pass_out(cn->router, cn->route_buf, cn->route_len, cn->channel, dest);
	cn->route_len = 0;
}

static void catn_route_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	struct cat_native *cn = w->data;
	(void)loop; (void)revents;
	catn_route_flush(cn, catn_route_default(cn));
}

static void catn_route_start(struct cat_native *cn) {
	if(cn->routing)
		return;
	cn->routing = 1;
	cn->route_len = 0;
	mhr_intercept_consumers(cn->router, cn->channel, 1);
}

static void catn_route_stop(struct cat_native *cn) {
	if(!cn->routing)
		return;
	catn_route_flush(cn, MHR_ALL);
	cn->routing = 0;
	mhr_intercept_consumers(cn->router, cn->channel, 0);
}

// A removed connector's command may still be answered.
static int catn_removed_busy(const struct cat_native *cn) {
	for(int i = 0; i < CAT_MAX_CLIENTS; i++) {
		int fd = cn->clients_fd[i];
		if(fd >= 0 && !mhr_get_producer_tag(cn->router, cn->channel, fd) && catn_client_busy(cn, i + 1))
			return 1;
	}
	return 0;
}

// Radio data is routed all the time with several connectors, with connectors that don't take
// transceive / AI, and with collision resend on CI-V.
static int catn_route_always(const struct cat_native *cn) {
	return cn->multi || cn->filter || (cn->civ_resend && cn->civ);
}

// Otherwise routing ends when no own query can be answered any more, nor a command of a
// connector just removed (late answers included).
static void catn_route_check(struct cat_native *cn) {
	if(cn->routing && !catn_route_always(cn) && !cn->own_active && !(catn_pending(cn) & CAT_PENDING_OWN) &&
	   !catn_removed_busy(cn))
		catn_route_stop(cn);
}

/*
 * Several applications
 */

static const char *catn_connector_name(const struct cat_native *cn, int fd) {
	const char *tag = mhr_get_producer_tag(cn->router, cn->channel, fd);
	return tag && *tag ? tag : "?";
}

// Client number of a connector, assigned when it first sends data. 0 if all are taken.
static int catn_client_of(struct cat_native *cn, int fd) {
	int i, free_slot = -1, removed_slot = -1;

	for(i = 0; i < CAT_MAX_CLIENTS; i++) {
		if(cn->clients_fd[i] == fd)
			return i + 1;
		if(free_slot < 0 && cn->clients_fd[i] < 0)
			free_slot = i;
		// The slot of a removed connector, if nothing else is left.
		if(removed_slot < 0 && cn->clients_fd[i] >= 0 &&
		   !mhr_get_producer_tag(cn->router, cn->channel, cn->clients_fd[i]))
			removed_slot = i;
	}
	if(free_slot < 0)
		free_slot = removed_slot;
	if(free_slot < 0)
		return 0;
	cn->clients_fd[free_slot] = fd;
	catn_mon_comment(cn, "H%d: connector %s", free_slot + 1, catn_connector_name(cn, fd));
	return free_slot + 1;
}

static void catn_turn_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);

// The application on connector fd gets the line, -1: free for all.
static void catn_turn_give(struct cat_native *cn, int fd, int fresh) {
	cn->turn_fd = fd;
	cn->turn_client = fd >= 0 ? catn_client_of(cn, fd) : 0;
	cn->turn_fresh = fresh;
	cn->turn_t = ev_now(cn->loop);
	mhr_set_producer_turn(cn->router, cn->channel, fd);
	if(fd >= 0)
		ev_timer_again(cn->loop, &cn->turn_timer);
	else
		ev_timer_stop(cn->loop, &cn->turn_timer);
}

// The turn ends when the radio answered the commands of the application, or it gave up on
// them. That holds for a connector removed meanwhile too: the answers to its last commands may
// still come, the next command would collide with them. Then the next application with data
// waiting gets the line, round robin; none: free for all, whoever sends first gets it.
static void catn_turn_check(struct cat_native *cn) {
	ev_tstamp now = ev_now(cn->loop);

	if(cn->turn_fd < 0)
		return;
	int fresh = cn->turn_fresh && mhr_get_producer_tag(cn->router, cn->channel, cn->turn_fd);
	int busy = fresh || catn_client_busy(cn, cn->turn_client);
	if(busy && catn_answering(cn, cn->turn_client))
		cn->turn_t = now;
	if(busy && now - cn->turn_t < CATN_TURN_TIMEOUT)
		return;
	if(busy && !fresh) {
		dbg0("%s %s native CAT: no answer to H%d within %.1f s, next application", cn->serial,
		     radio_str(cn->radio), cn->turn_client, CATN_TURN_TIMEOUT);
		catn_mon_comment(cn, "H%d: no answer within %.1f s, its commands are given up",
				 cn->turn_client, CATN_TURN_TIMEOUT);
		catn_client_discard(cn, cn->turn_client);
	}
	int next = cn->multi ? mhr_next_waiting_producer(cn->router, cn->channel, cn->turn_fd) : -1;
	catn_turn_give(cn, next, next >= 0);
}

static void catn_turn_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	(void)loop; (void)revents;
	catn_turn_check(w->data);
}

// Connectors with more than one session share one data stream: their applications can't be
// told apart.
static void catn_check_maxcon_cb(const struct con_info *info, void *user_data) {
	struct cat_native *cn = user_data;

	if(info->channel != cn->channel || info->maxcon <= 1 || !info->serial || strcmp(info->serial, cn->serial))
		return;
	warn("%s %s native CAT: %s%s accepts %d connections. mhuxd can't tell the applications sharing it "
	     "apart, nor take turns between them: give each application a port of its own", cn->serial,
	     radio_str(cn->radio), info->type == CON_TCP ? "TCP port " : "/dev/mhuxd/",
	     info->devname ? info->devname : "?", info->maxcon);
}

// Connectors that don't take transceive / AI (forward_unsolicited false): the radio data is
// routed, so these frames can be held back from them. Needs a decoder.
static void catn_filter_update(struct cat_native *cn) {
	int n = mhr_count_consumers_flagged(cn->router, cn->channel, MHR_CNS_NO_UNSOLICITED);
	int decoder = catn_has_decoder(cn);
	int filter = n > 0 && decoder;

	if(n && !decoder && cn->rigtype >= 0 && !cn->filter_warned)
		warn("%s %s native CAT: no decoder for rig type '%s', transceive and auto information "
		     "reach all ports", cn->serial, radio_str(cn->radio), rtyp_get_name(cn->rigtype));
	cn->filter_warned = n && !decoder && cn->rigtype >= 0;
	if(filter && (!cn->filter || n != cn->no_unsolicited)) {
		info("%s %s native CAT: %d port(s) without transceive and auto information", cn->serial,
		     radio_str(cn->radio), n);
		catn_mon_comment(cn, "%d port(s) without transceive and auto information", n);
	}
	cn->no_unsolicited = n;
	cn->filter = filter;
}

// Taking turns needs more than one connector and a decoder.
static void catn_multi_update(struct cat_native *cn) {
	int n = mhr_count_producers(cn->router, cn->channel);
	int multi = n > 1 && catn_has_decoder(cn);

	catn_filter_update(cn);
	if(n != cn->connectors) {
		cn->connectors = n;
		if(cn->ctx)
			conmgr_foreach(app_ctx_get_conmgr(cn->ctx), catn_check_maxcon_cb, cn);
	}
	if(multi != cn->multi) {
		cn->multi = multi;
		if(multi) {
			info("%s %s native CAT: %d connectors, taking turns between their applications",
			     cn->serial, radio_str(cn->radio), n);
			catn_mon_comment(cn, "%d connectors, taking turns", n);
		} else {
			info("%s %s native CAT: %s", cn->serial, radio_str(cn->radio),
			     n > 1 ? "no decoder, not taking turns between the applications" : "single connector");
			catn_mon_comment(cn, "%d connector(s), not taking turns", n);
			// With a decoder, a turn in progress ends when its commands are answered.
			if(!catn_has_decoder(cn))
				catn_turn_give(cn, -1, 0);
		}
	}
	if(catn_route_always(cn))
		catn_route_start(cn);
	else
		catn_route_check(cn);
}

/*
 * Own queries
 */

static void catn_own_end(struct cat_native *cn) {
	if(!cn->own_active)
		return;
	cn->own_active = 0;
	ev_timer_stop(cn->loop, &cn->own_timer);
	mhr_hold_producers(cn->router, cn->channel, 0);
	// The application next in turn waited for the query: its time starts now.
	if(cn->turn_fd >= 0 && cn->turn_fresh)
		cn->turn_t = ev_now(cn->loop);
}

// Decoder starts over: what the radio knows and doesn't know is learned again.
static void catn_own_reset(struct cat_native *cn) {
	catn_own_end(cn);
	if(!catn_route_always(cn))
		catn_route_stop(cn);
	memset(cn->own_sent, 0, sizeof(cn->own_sent));
	memset(cn->own_rejects, 0, sizeof(cn->own_rejects));
	memset(cn->own_unsupported, 0, sizeof(cn->own_unsupported));
	cn->civ_mode_cmd = 0x26;
	cn->own_cat_on = 0;
	cn->own_timeouts = 0;
	cn->own_last = 0;
}

static void catn_reset(struct cat_native *cn) {
	cat_state_init(&cn->st);
	ev_timer_stop(cn->loop, &cn->publish_timer);
	ev_timer_stop(cn->loop, &cn->keepalive_timer);
	ev_timer_stop(cn->loop, &cn->resend_timer);
	cn->data_unknown_since = 0;
	cn->last_info_valid = 0;
	catn_own_reset(cn);
	catn_turn_give(cn, -1, 0);	// the decoder's view of the applications' commands is gone
}

static void catn_frame_cb(const struct cat_frame *f, void *user_data);

static void catn_setup_decoder(struct cat_native *cn, int rigtype, int addr);

// (Re)creates the decoder when the channel's rig type or CI-V address changed.
static void catn_setup(struct cat_native *cn, int force) {
	ev_tstamp now = ev_now(cn->loop);
	struct mhc_speed_cfg sp;
	int rigtype = -1, addr = 0, decoder = 0;

	if(!force && now - cn->cfg_checked < CATN_CFG_CHECK_IVAL)
		return;
	cn->cfg_checked = now;

	if(mhc_get_speed_params(cn->ctl, cn->channel, &sp) == 0) {
		rigtype = sp.rigtype;
		addr = sp.icomaddress;
		decoder = sp.usedecoderifconnected;
	}
	if(cn->civ_address)
		addr = cn->civ_address;

	if(decoder && !cn->decoder_warned)
		warn("%s %s native CAT: keyer option 'use decoder if connected' is on, the keyer's own "
		     "CAT queries collide with the applications' commands", cn->serial, radio_str(cn->radio));
	cn->decoder_warned = decoder;

	if(rigtype == cn->rigtype && addr == cn->civ_addr)
		return;
	catn_setup_decoder(cn, rigtype, addr);
	catn_multi_update(cn);
}

static void catn_setup_decoder(struct cat_native *cn, int rigtype, int addr) {
	cn->rigtype = rigtype;
	cn->civ_addr = addr;

	catn_destroy_decoder(cn);
	catn_reset(cn);

	if(rigtype < 0) {
		dbg0("%s %s native CAT: channel not configured yet", cn->serial, radio_str(cn->radio));
		return;
	}

	int no_data, ybin_model;
	int protocol = protocol_of_rig(rigtype, &no_data, &ybin_model);
	if(cn->protocol != CAT_NATIVE_PROTO_AUTO)
		protocol = cn->protocol;

	char line[64];
	if(protocol == CAT_NATIVE_PROTO_CIV) {
		struct cat_civ_cfg cc = {
			.radio_addr = addr,
			.freq_len = rigtype == RIG_TYPE_IC735 ? 4 : 0,
		};
		cn->civ = cat_civ_create(&cc, &cn->st);
		cat_trace_civ_config(line, sizeof(line), &cc);
	} else if(protocol == CAT_NATIVE_PROTO_YBIN) {
		struct cat_ybin_cfg yc = { .model = ybin_model };
		cn->ybin = cat_ybin_create(&yc, &cn->st);
		cat_trace_ybin_config(line, sizeof(line), &yc);
	} else if(protocol != CAT_NATIVE_PROTO_NONE) {
		struct cat_ascii_cfg ac = {
			.dialect = protocol == CAT_NATIVE_PROTO_ELECRAFT ? CAT_ASCII_ELECRAFT :
				   protocol == CAT_NATIVE_PROTO_YAESU ? CAT_ASCII_YAESU :
				   protocol == CAT_NATIVE_PROTO_FLEX ? CAT_ASCII_FLEX : CAT_ASCII_KENWOOD,
			.no_data = no_data,
		};
		cn->ascii = cat_ascii_create(&ac, &cn->st);
		cat_trace_ascii_config(line, sizeof(line), &ac);
	} else {
		info("%s %s native CAT: no decoder for rig type '%s' yet, use rigctld",
		     cn->serial, radio_str(cn->radio), rtyp_get_name(rigtype));
		catn_mon_comment(cn, "no decoder for rig type '%s'", rtyp_get_name(rigtype));
		return;
	}

	if(!catn_has_decoder(cn)) {
		err("%s %s native CAT: out of memory", cn->serial, radio_str(cn->radio));
		return;
	}
	cn->decoder_protocol = protocol;
	cn->no_data = no_data;
	if(cn->civ)
		cat_civ_set_frame_cb(cn->civ, catn_frame_cb, cn);
	else if(cn->ascii)
		cat_ascii_set_frame_cb(cn->ascii, catn_frame_cb, cn);
	else
		cat_ybin_set_frame_cb(cn->ybin, catn_frame_cb, cn);
	if(cn->civ)
		info("%s %s native CAT: CI-V, rig type '%s', radio address %02X%s", cn->serial,
		     radio_str(cn->radio), rtyp_get_name(rigtype), addr, addr ? "" : " (learn)");
	else if(cn->ybin)
		info("%s %s native CAT: Yaesu binary protocol, rig type '%s' (%s)", cn->serial,
		     radio_str(cn->radio), rtyp_get_name(rigtype), cat_ybin_model_name(ybin_model));
	else
		info("%s %s native CAT: %s, rig type '%s'%s", cn->serial, radio_str(cn->radio),
		     cat_native_protocol_str(protocol), rtyp_get_name(rigtype),
		     no_data && protocol == CAT_NATIVE_PROTO_KENWOOD ? ", no data mode" : "");

	if(cn->monitor) {
		catn_mon_comment(cn, "rig type '%s'", rtyp_get_name(rigtype));
		catn_mon_line(cn, line);
	}
}

static int info_differs(const struct mhc_radio_info *a, const struct mhc_radio_info *b) {
	return a->mode != b->mode || a->rxFreq != b->rxFreq || a->txFreq != b->txFreq ||
	       a->vfoAFreq != b->vfoAFreq || a->vfoBFreq != b->vfoBFreq;
}

static void catn_publish(struct cat_native *cn) {
	const struct cat_vfo_state *sel = &cn->st.vfo[CAT_VFO_SEL];
	struct mhc_radio_info info;

	if(cat_state_to_radio_info(&cn->st, cn->radio, &info))
		return;

	if(sel->mode.valid && !sel->data.valid && cat_mode_with_data(sel->mode.val, 1) != sel->mode.val) {
		ev_tstamp now = ev_now(cn->loop);
		if(!cn->data_unknown_since)
			cn->data_unknown_since = now;
		ev_tstamp wait = cn->data_unknown_since + CATN_DATA_WAIT - now;
		if(wait > 0) {
			info.mode = -1;
			ev_timer_set(&cn->publish_timer, wait, 0.);
			ev_timer_start(cn->loop, &cn->publish_timer);
		}
	} else {
		cn->data_unknown_since = 0;
	}

	if(!cn->last_info_valid || info_differs(&info, &cn->last_info)) {
		enum cat_mode m = cat_state_mode(&cn->st, CAT_VFO_SEL);
		dbg0("%s %s native CAT: rx=%u tx=%u vfoA=%u vfoB=%u mode=%s keyer mode=%d%s", cn->serial,
		     radio_str(cn->radio), info.rxFreq, info.txFreq, info.vfoAFreq, info.vfoBFreq,
		     m ? cat_mode_token(m) : "?", info.mode, info.mode < 0 && m ? " (waiting for data flag)" : "");
	}
	cn->last_info = info;
	cn->last_info_valid = 1;
	mhc_update_radio_info(cn->ctl, CATN_SOURCE, &info);
	ev_timer_again(cn->loop, &cn->keepalive_timer);
}

static void catn_publish_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	(void)loop; (void)revents;
	catn_publish(w->data);
}

// Own queries can be sent: configured, decoder with a known radio address, keyer online.
static int catn_can_query(const struct cat_native *cn) {
	if(!cn->own_queries || !catn_has_decoder(cn))
		return 0;
	if(cn->civ && !cat_civ_radio_addr(cn->civ))
		return 0;
	return mhc_get_state(cn->ctl) == MHC_KEYER_STATE_ONLINE;
}

static void catn_keepalive_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	struct cat_native *cn = w->data;
	(void)revents;

	if(!cn->last_info_valid)
		return;
	if(catn_can_query(cn) && ev_now(loop) - cn->last_radio > CATN_RADIO_SILENT) {
		if(!cn->radio_silent)
			info("%s %s native CAT: no answer from the radio for %.0f s, radio state expires",
			     cn->serial, radio_str(cn->radio), CATN_RADIO_SILENT);
		cn->radio_silent = 1;
		return;
	}
	dbg1("%s %s native CAT: no new data, sending the last state again", cn->serial, radio_str(cn->radio));
	mhc_update_radio_info(cn->ctl, CATN_SOURCE, &cn->last_info);
}

static void catn_feed(struct cat_native *cn, enum cat_dir dir, int client, unsigned const char *data, int len) {
	catn_setup(cn, 0);
	const struct cat_stats *stats = catn_stats(cn);
	if(!stats || len <= 0)
		return;

	uint32_t applied = stats->applied;
	uint64_t t_ms = (uint64_t)(ev_now(cn->loop) * 1000.0);
	catn_decoder_feed(cn, dir, client, data, len, t_ms);
	if(stats->applied == applied)
		return;

	ev_timer *t = &cn->publish_timer;
	if(!ev_is_active(t) || ev_timer_remaining(cn->loop, t) > CATN_PUBLISH_DELAY) {
		ev_timer_stop(cn->loop, t);
		ev_timer_set(t, CATN_PUBLISH_DELAY, 0.);
		ev_timer_start(cn->loop, t);
	}
}

// Frame for an own query, length 0 if the protocol has none for this value. *answer: length of
// the answer if known, else 0.
static size_t catn_build_query(const struct cat_native *cn, int slot, uint8_t *buf, size_t *answer) {
	const char *q = NULL;

	*answer = 0;
	if(cn->ybin) {
		if(slot != Q_FREQ)
			return 0;
		*answer = cat_ybin_query(cn->ybin, buf);
		return *answer ? YBIN_CMD_LEN : 0;
	}

	if(cn->civ) {
		size_t n = 0;
		buf[n++] = 0xfe;
		buf[n++] = 0xfe;
		buf[n++] = cat_civ_radio_addr(cn->civ);
		buf[n++] = CATN_CIV_OWN_ADDR;
		switch(slot) {
		case Q_FREQ:
			buf[n++] = 0x03;
			break;
		case Q_MODE:
			buf[n++] = cn->civ_mode_cmd;
			if(cn->civ_mode_cmd == 0x26)
				buf[n++] = 0x00;	// selected VFO
			break;
		default:
			buf[n++] = 0x1a;
			buf[n++] = 0x06;
			break;
		}
		buf[n++] = 0xfd;
		return n;
	}

	switch(cn->decoder_protocol) {
	case CAT_NATIVE_PROTO_FLEX:
		q = slot == Q_FREQ ? "FA;" : slot == Q_MODE ? "MD;" : NULL;
		break;
	case CAT_NATIVE_PROTO_YAESU:
		q = slot == Q_FREQ ? "IF;" : NULL;
		break;
	case CAT_NATIVE_PROTO_ELECRAFT:
		q = slot == Q_FREQ ? "IF;" : slot == Q_DATA ? "DT;" : NULL;
		break;
	default:
		if(slot == Q_FREQ)
			q = "IF;";
		else if(slot == Q_DATA && !cn->no_data)
			q = cn->rigtype == RIG_TYPE_TS950 ? "DT;" : "DA;";
		break;
	}
	if(!q)
		return 0;
	memcpy(buf, q, strlen(q));
	return strlen(q);
}

static int item_stale(const struct cat_item *it, uint64_t now_ms) {
	return !it->valid || it->t_ms + (uint64_t)(CATN_OWN_STALE * 1000.0) < now_ms;
}

// The value behind the query is unknown or older than CATN_OWN_STALE.
static int catn_query_needed(const struct cat_native *cn, int slot, uint64_t now_ms) {
	const struct cat_vfo_state *v = &cn->st.vfo[CAT_VFO_SEL];
	int freq = item_stale(&v->freq, now_ms);
	int mode = item_stale(&v->mode, now_ms);
	int data = v->mode.valid && cat_mode_with_data(v->mode.val, 1) != (enum cat_mode)v->mode.val &&
		   item_stale(&v->data, now_ms);

	if(cn->civ) {
		switch(slot) {
		case Q_FREQ: return freq;
		case Q_MODE: return mode || (data && cn->civ_mode_cmd == 0x26);
		default:     return data && !mode && cn->civ_mode_cmd == 0x04;
		}
	}

	switch(slot) {
	case Q_FREQ:
		return freq || (mode && cn->decoder_protocol != CAT_NATIVE_PROTO_FLEX);
	case Q_MODE:
		return mode;
	default:
		if(cn->decoder_protocol == CAT_NATIVE_PROTO_ELECRAFT) {
			// DATA mode: the sub mode (DT) only changes on the radio, IF doesn't tell it.
			enum cat_mode m = cat_state_mode(&cn->st, CAT_VFO_SEL);
			return m == CAT_MODE_PKTUSB || m == CAT_MODE_PKTLSB || m == CAT_MODE_RTTY ||
			       m == CAT_MODE_RTTYR;
		}
		return data;
	}
}

static void catn_own_send(struct cat_native *cn, int slot) {
	uint8_t buf[16];
	size_t answer;
	size_t len = catn_build_query(cn, slot, buf, &answer);
	ev_tstamp now = ev_now(cn->loop);

	cn->own_sent[slot] = now;
	cn->own_last = now;
	if(!len) {
		cn->own_unsupported[slot] = 1;
		return;
	}

	// The applications' data waits until the answer is in or the query timed out.
	mhr_hold_producers(cn->router, cn->channel, 1);
	if(mhr_send_in(cn->router, buf, len, cn->channel) != (int)len) {
		mhr_hold_producers(cn->router, cn->channel, 0);
		return;
	}
	dbg1("%s %s native CAT: own %s query", cn->serial, radio_str(cn->radio), q_names[slot]);
	cn->own_active = 1;
	cn->own_slot = slot;
	// Long answers of the older Yaesu radios: 75 bytes take 170 ms at 4800 Bd.
	cn->own_timeout = CATN_OWN_TIMEOUT + answer * CATN_OWN_BYTE_TIME;
	catn_route_start(cn);
	ev_timer_set(&cn->own_timer, cn->own_timeout, 0.);
	ev_timer_start(cn->loop, &cn->own_timer);

	catn_decoder_feed(cn, CAT_DIR_OWN, 0, buf, len, (uint64_t)(now * 1000.0));
}

// Answer to the own query in flight: reply, acknowledge or rejection.
static void catn_own_answered(struct cat_native *cn, const struct cat_frame *f) {
	int slot = cn->own_slot;

	catn_own_end(cn);
	cn->own_timeouts = 0;
	if(f->kind != CAT_FRAME_NAK && f->kind != CAT_FRAME_ERROR) {
		cn->own_rejects[slot] = 0;
		return;
	}

	// A single rejection may be the radio's state (TX, menu), not the command.
	if(++cn->own_rejects[slot] < CATN_OWN_MAX_REJECTS)
		return;
	cn->own_rejects[slot] = 0;
	if(cn->civ && slot == Q_MODE && cn->civ_mode_cmd == 0x26) {
		info("%s %s native CAT: the radio doesn't know CI-V command 26, asking 04 and 1A 06 instead",
		     cn->serial, radio_str(cn->radio));
		cn->civ_mode_cmd = 0x04;
		return;
	}
	info("%s %s native CAT: the radio rejects the %s query, not asked again", cn->serial,
	     radio_str(cn->radio), q_names[slot]);
	cn->own_unsupported[slot] = 1;
}

static void catn_own_timeout_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	struct cat_native *cn = w->data;
	(void)loop; (void)revents;

	if(catn_answering(cn, 0)) {
		ev_timer_set(&cn->own_timer, CATN_TURN_TICK, 0.);
		ev_timer_start(cn->loop, &cn->own_timer);
		return;
	}
	// Late answers are still taken out while the decoder keeps the query pending.
	catn_own_end(cn);
	cn->own_timeouts++;
	dbg0("%s %s native CAT: no answer to own %s query", cn->serial, radio_str(cn->radio),
	     q_names[cn->own_slot]);
	if(cn->own_timeouts == CATN_OWN_MAX_TIMEOUTS)
		info("%s %s native CAT: the radio doesn't answer, asking every %.0f s", cn->serial,
		     radio_str(cn->radio), CATN_OWN_RETRY);
}

// The FT-847 ignores everything until CAT ON and forgets it when switched off, or when an
// application sends CAT OFF (Hamlib does when it closes). Sent before the first own query and
// again before the next one after a query went unanswered; the query follows with a later
// try, once the decoder's hold after the command (YBIN_HOLD_MS) is over. urouter sends it
// only once, at its start. Returns 1 if sent.
static int catn_own_cat_on(struct cat_native *cn) {
	uint8_t buf[YBIN_CMD_LEN];
	ev_tstamp now = ev_now(cn->loop);
	size_t len;

	if(!cn->ybin || (cn->own_cat_on && (!cn->own_timeouts || cn->own_cat_on > cn->own_last)))
		return 0;
	if(!(len = cat_ybin_cat_on(cn->ybin, buf)))
		return 0;
	cn->own_cat_on = now;
	if(mhr_send_in(cn->router, buf, len, cn->channel) != (int)len)
		return 1;
	dbg1("%s %s native CAT: own CAT ON", cn->serial, radio_str(cn->radio));
	catn_decoder_feed(cn, CAT_DIR_OWN, 0, buf, len, (uint64_t)(now * 1000.0));
	return 1;
}

// Sends the next own query that is due, if the line is free: no command of an application
// waiting for its answer, none partly sent, no earlier query that may still be answered. An
// application that got the line but hasn't sent yet waits for the query (its data is held).
static void catn_own_try(struct cat_native *cn) {
	ev_tstamp now = ev_now(cn->loop);

	if(cn->own_active || !catn_can_query(cn) || catn_pending(cn) || (cn->turn_fd >= 0 && !cn->turn_fresh))
		return;
	if(cn->own_timeouts >= CATN_OWN_MAX_TIMEOUTS && now - cn->own_last < CATN_OWN_RETRY)
		return;

	uint64_t now_ms = (uint64_t)(now * 1000.0);
	for(int slot = 0; slot < Q_NUM; slot++) {
		if(cn->own_unsupported[slot] || now - cn->own_sent[slot] < CATN_OWN_STALE)
			continue;
		if(catn_query_needed(cn, slot, now_ms)) {
			if(!catn_own_cat_on(cn))
				catn_own_send(cn, slot);
			return;
		}
	}
}

static void catn_tick_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	struct cat_native *cn = w->data;
	ev_tstamp now = ev_now(loop);
	(void)revents;

	if(cn->ybin)
		cat_ybin_expire(cn->ybin, (uint64_t)(now * 1000.0));
	catn_multi_update(cn);
	catn_route_check(cn);
	if(cn->own_active)
		return;
	catn_setup(cn, 0);
	// Between the applications' commands, queries go out when one is answered (catn_radio_cb).
	// Here: nobody talks, or only traffic the decoder doesn't follow (another controller on the
	// bus, an address the decoder doesn't know) for a moment.
	if(now - cn->last_host < CATN_OWN_IDLE || now - cn->last_radio < CATN_OWN_IDLE)
		return;
	catn_own_try(cn);
}

// Sends a command destroyed in a bus collision again, see CATN_RESEND_QUIET.
static void catn_civ_resend(struct cat_native *cn) {
	uint8_t buf[CIV_MAX_FRAME];
	int client = 0, own = 0, lost;
	ev_tstamp now = ev_now(cn->loop);

	ev_timer_stop(cn->loop, &cn->resend_timer);
	if(!cn->civ || !(lost = cat_civ_lost(cn->civ)))
		return;
	size_t len = cat_civ_resend(cn->civ, CATN_RESEND_MAX, (uint64_t)(now * 1000.0), buf, &client, &own);
	if(!len) {
		dbg0("%s %s native CAT: command lost in a collision again, given up", cn->serial, radio_str(cn->radio));
		catn_mon_comment(cn, "collision: given up after %d resends", CATN_RESEND_MAX);
		return;
	}
	// An own query that timed out meanwhile: the applications may be on the line again.
	if(own && !cn->own_active)
		return;
	if(mhr_send_in(cn->router, buf, len, cn->channel) != (int)len)
		return;
	if(own) {
		ev_timer_stop(cn->loop, &cn->own_timer);
		ev_timer_set(&cn->own_timer, cn->own_timeout, 0.);
		ev_timer_start(cn->loop, &cn->own_timer);
	} else if(client && client == cn->turn_client) {
		cn->turn_t = now;
	}
	dbg1("%s %s native CAT: collision, command sent again", cn->serial, radio_str(cn->radio));
	if(own)
		catn_mon_comment(cn, "M: collision, query sent again");
	else
		catn_mon_comment(cn, "H%d: collision, command sent again", client);
	if(lost > 1)
		ev_timer_again(cn->loop, &cn->resend_timer);
}

static void catn_resend_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	(void)loop; (void)revents;
	catn_civ_resend(w->data);
}

static void catn_frame_cb(const struct cat_frame *f, void *user_data) {
	struct cat_native *cn = user_data;

	if(cn->monitor)
		catn_mon_frame(cn, f);
	if(f->dir != CAT_DIR_RADIO)
		return;
	int jam = f->kind == CAT_FRAME_JAM || f->kind == CAT_FRAME_DROPPED;
	if(!jam)
		cn->radio_frame_done = 1;
	if(f->own) {
		if(cn->own_active && f->kind != CAT_FRAME_ECHO)
			catn_own_answered(cn, f);
		if(cn->route_dest == ROUTE_NONE)
			cn->route_dest = ROUTE_DROP;	// not for the applications
		return;
	}
	// Echo and answers to the application that sent the command, transceive and auto
	// information to all that take it, anything else to the application whose turn it is.
	int dest = f->kind == CAT_FRAME_UNSOLICITED ? MHR_UNSOLICITED : catn_route_default(cn);
	if(f->client > 0 && cn->clients_fd[f->client - 1] >= 0)
		dest = cn->clients_fd[f->client - 1];
	// Collision: mhuxd sends the destroyed command again, the debris is no use to anybody.
	if(jam && cn->civ_resend && cn->civ && cat_civ_lost(cn->civ))
		dest = ROUTE_DROP;
	cn->route_dest = dest;
	// An application's query answered: the line is free. Not after FB, a radio with transceive
	// on may report the change that was just set.
	if(f->client > 0 && (f->kind == CAT_FRAME_REPLY || f->kind == CAT_FRAME_NAK || f->kind == CAT_FRAME_ERROR))
		cn->client_answered = 1;
}

// Radio -> host, one byte per call.
static void catn_radio_cb(struct mh_router *router, unsigned const char *data, int len, int channel,
			  void *user_data) {
	struct cat_native *cn = user_data;
	(void)router; (void)channel;

	cn->last_radio = ev_now(cn->loop);
	if(cn->radio_silent) {
		info("%s %s native CAT: data from the radio again", cn->serial, radio_str(cn->radio));
		cn->radio_silent = 0;
	}

	for(int i = 0; i < len; i++) {
		cn->client_answered = 0;
		if(!cn->routing) {
			// The router passed the data on to the applications.
			catn_feed(cn, CAT_DIR_RADIO, 0, &data[i], 1);
			catn_turn_check(cn);
			// The line is free until the application's next command: an own query may go in.
			if(cn->client_answered)
				catn_own_try(cn);
			continue;
		}
		if(cn->route_len == CATN_ROUTE_MAX)
			catn_route_flush(cn, catn_route_default(cn));
		cn->route_buf[cn->route_len++] = data[i];
		cn->route_dest = ROUTE_NONE;
		cn->radio_frame_done = 0;
		catn_feed(cn, CAT_DIR_RADIO, 0, &data[i], 1);
		if(cn->route_dest != ROUTE_NONE)
			catn_route_flush(cn, cn->route_dest);
		else
			ev_timer_again(cn->loop, &cn->route_timer);
		// A command lost in a collision goes out again when the radio has just finished a
		// frame, or has been quiet for a moment.
		if(cn->civ_resend && cn->civ && cat_civ_lost(cn->civ)) {
			if(cn->radio_frame_done)
				catn_civ_resend(cn);
			else
				ev_timer_again(cn->loop, &cn->resend_timer);
		}
		// Answered: the next application may send, after an own query if one is due. Its data
		// is read after this callback.
		catn_turn_check(cn);
		if(cn->client_answered)
			catn_own_try(cn);
		catn_route_check(cn);
		if(!cn->routing && i + 1 < len)
			mhr_pass_out(cn->router, &data[i + 1], len - i - 1, cn->channel, MHR_ALL);
	}
}

// Host -> radio, as the router reads it from the connectors.
static void catn_host_cb(struct mh_router *router, unsigned const char *data, int len, int channel,
			 int fd, void *user_data) {
	struct cat_native *cn = user_data;
	(void)router; (void)channel;

	cn->last_host = ev_now(cn->loop);
	catn_multi_update(cn);
	// The first application to send gets the line; the router holds the others' data.
	if(cn->multi && cn->turn_fd < 0)
		catn_turn_give(cn, fd, 0);
	if(fd == cn->turn_fd) {
		cn->turn_fresh = 0;
		cn->turn_t = cn->last_host;
	}
	catn_feed(cn, CAT_DIR_HOST, catn_client_of(cn, fd), data, len);
}

static void catn_keyer_state_cb(enum app_event_type type, const void *data, void *user_data) {
	struct cat_native *cn = user_data;
	const struct ev_keyer_state *ev = data;

	if(type != EV_KEYER_STATE || !ev || (ev->serial && strcmp(ev->serial, cn->serial)))
		return;

	// Values decoded before the keyer went away may be outdated: start over.
	catn_mon_comment(cn, "keyer %s, decoder starts over", mhc_state_str(ev->state));
	catn_destroy_decoder(cn);
	cn->rigtype = -1;
	cn->civ_addr = -1;
	catn_reset(cn);
	catn_multi_update(cn);
	if(ev->state == MHC_KEYER_STATE_ONLINE)
		catn_setup(cn, 1);
}

struct cat_native *cat_native_create(struct app_ctx *ctx, struct device *dev, const struct cat_native_cfg *cfg) {
	if(!ctx || !dev || !dev->ctl || !dev->router || !dev->serial || !cfg)
		return NULL;

	struct cat_native *cn = w_calloc(1, sizeof(*cn));
	cn->loop = app_ctx_get_loop(ctx);
	cn->ctl = dev->ctl;
	cn->router = dev->router;
	cn->serial = w_strdup(dev->serial);
	cn->radio = cfg->radio == 2 ? 2 : 1;
	cn->channel = cn->radio == 2 ? MH_CHANNEL_R2 : MH_CHANNEL_R1;
	cn->protocol = cfg->protocol;
	cn->civ_address = cfg->civ_address;
	cn->own_queries = cfg->own_queries;
	cn->civ_resend = cfg->civ_resend;
	cn->civ_mode_cmd = 0x26;
	cn->ebus = app_ctx_get_eventbus(ctx);
	cn->monitor = cfg->monitor;
	if(cn->monitor)
		catn_mon_open(cn, cfg->monitor_dir ? cfg->monitor_dir : LOGDIR);
	cn->rigtype = -1;
	cn->civ_addr = -1;
	cat_state_init(&cn->st);
	cn->ctx = ctx;
	for(int i = 0; i < CAT_MAX_CLIENTS; i++)
		cn->clients_fd[i] = -1;
	cn->turn_fd = -1;

	ev_timer_init(&cn->publish_timer, catn_publish_cb, 0., 0.);
	cn->publish_timer.data = cn;
	ev_timer_init(&cn->keepalive_timer, catn_keepalive_cb, 0., CATN_KEEPALIVE);
	cn->keepalive_timer.data = cn;
	ev_timer_init(&cn->own_timer, catn_own_timeout_cb, 0., 0.);
	cn->own_timer.data = cn;
	ev_timer_init(&cn->tick, catn_tick_cb, CATN_TICK, CATN_TICK);
	cn->tick.data = cn;
	ev_timer_start(cn->loop, &cn->tick);
	ev_timer_init(&cn->turn_timer, catn_turn_timer_cb, 0., CATN_TURN_TICK);
	cn->turn_timer.data = cn;
	ev_timer_init(&cn->route_timer, catn_route_timer_cb, 0., CATN_ROUTE_FLUSH);
	cn->route_timer.data = cn;
	ev_timer_init(&cn->resend_timer, catn_resend_timer_cb, 0., CATN_RESEND_QUIET);
	cn->resend_timer.data = cn;

	mhr_add_consumer_cb(cn->router, catn_radio_cb, cn->channel, cn);
	mhr_add_producer_tap_cb(cn->router, catn_host_cb, cn->channel, cn);
	cn->kscb = eventbus_subscribe(app_ctx_get_eventbus(ctx), EV_KEYER_STATE, catn_keyer_state_cb, cn);

	info("%s %s native CAT rig mode sync created (protocol %s%s%s%s)", cn->serial, radio_str(cn->radio),
	     cat_native_protocol_str(cn->protocol), cn->own_queries ? ", own queries" : "",
	     cn->civ_resend ? ", CI-V collision resend" : "", cn->monitor ? ", monitor on" : "");
	catn_setup(cn, 1);
	return cn;
}

void cat_native_destroy(struct cat_native *cn) {
	if(!cn)
		return;

	if(cn->kscb)
		eventbus_unsubscribe(cn->kscb);
	// Releases held data and the radio data.
	catn_own_reset(cn);
	catn_turn_give(cn, -1, 0);
	catn_route_stop(cn);
	mhr_rem_producer_tap_cb(cn->router, catn_host_cb, cn->channel, cn);
	mhr_rem_consumer_cb(cn->router, catn_radio_cb, cn->channel);
	ev_timer_stop(cn->loop, &cn->publish_timer);
	ev_timer_stop(cn->loop, &cn->keepalive_timer);
	ev_timer_stop(cn->loop, &cn->tick);
	ev_timer_stop(cn->loop, &cn->resend_timer);
	catn_destroy_decoder(cn);
	if(cn->mon_file) {
		catn_mon_comment(cn, "monitor stopped");
		fclose(cn->mon_file);
	}
	free(cn->mon_path);
	info("%s %s native CAT rig mode sync destroyed", cn->serial, radio_str(cn->radio));
	free(cn->serial);
	free(cn);
}
