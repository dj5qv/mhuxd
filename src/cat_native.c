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
#include "cat_civ.h"
#include "cat_state.h"
#include "cat_trace.h"
#include "app_ctx.h"
#include "channel.h"
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

// Rig types (keyer protocol keys, src/radiotypes.c) that need special handling.
#define RIG_TYPE_IC735   0x08	// 4-byte frequencies
#define RIG_TYPE_OMNI_VI 0x38	// Ten-Tec, speaks CI-V

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
	struct cat_civ *civ;
	struct cat_state st;
	ev_tstamp cfg_checked;
	int decoder_warned;

	ev_timer publish_timer;
	ev_timer keepalive_timer;
	ev_tstamp data_unknown_since;	// 0 = data flag known or irrelevant
	struct mhc_radio_info last_info;
	int last_info_valid;
	eventbus_sub_t *kscb;
};

static const char *radio_str(uint8_t radio) {
	return radio == 2 ? "r2" : "r1";
}

int cat_native_protocol_from_str(const char *s) {
	if(!s || !strcmp(s, "auto"))
		return CAT_NATIVE_PROTO_AUTO;
	if(!strcmp(s, "civ"))
		return CAT_NATIVE_PROTO_CIV;
	return -1;
}

static int is_civ_rig(int rigtype) {
	return rtyp_get_icom_address(rigtype) != 0 || rigtype == RIG_TYPE_OMNI_VI;
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

static void catn_mon_comment(struct cat_native *cn, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void catn_mon_comment(struct cat_native *cn, const char *fmt, ...) {
	char line[256];
	va_list ap;

	if(!cn->monitor)
		return;
	line[0] = '#';
	line[1] = ' ';
	va_start(ap, fmt);
	vsnprintf(line + 2, sizeof(line) - 2, fmt, ap);
	va_end(ap);
	catn_mon_line(cn, line);
}

static void catn_mon_frame_cb(const struct cat_civ_frame *f, void *user_data) {
	struct cat_native *cn = user_data;
	char time_str[32], line[CAT_TRACE_LINE_MAX];
	ev_tstamp now = ev_now(cn->loop);
	time_t secs = (time_t)now;
	struct tm tm;

	localtime_r(&secs, &tm);
	snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec,
		 (int)((now - (ev_tstamp)secs) * 1000.0));
	cat_trace_civ_frame(line, sizeof(line), time_str, "H1", f, &cn->st);
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

static void catn_reset(struct cat_native *cn) {
	cat_state_init(&cn->st);
	ev_timer_stop(cn->loop, &cn->publish_timer);
	ev_timer_stop(cn->loop, &cn->keepalive_timer);
	cn->data_unknown_since = 0;
	cn->last_info_valid = 0;
}

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
	cn->rigtype = rigtype;
	cn->civ_addr = addr;

	cat_civ_destroy(cn->civ);
	cn->civ = NULL;
	catn_reset(cn);

	if(rigtype < 0) {
		dbg0("%s %s native CAT: channel not configured yet", cn->serial, radio_str(cn->radio));
		return;
	}

	if(cn->protocol != CAT_NATIVE_PROTO_CIV && !is_civ_rig(rigtype)) {
		info("%s %s native CAT: no decoder for rig type '%s' yet, use rigctld",
		     cn->serial, radio_str(cn->radio), rtyp_get_name(rigtype));
		catn_mon_comment(cn, "no decoder for rig type '%s'", rtyp_get_name(rigtype));
		return;
	}

	struct cat_civ_cfg cc = {
		.radio_addr = addr,
		.freq_len = rigtype == RIG_TYPE_IC735 ? 4 : 0,
	};
	cn->civ = cat_civ_create(&cc, &cn->st);
	if(!cn->civ) {
		err("%s %s native CAT: out of memory", cn->serial, radio_str(cn->radio));
		return;
	}
	info("%s %s native CAT: CI-V, rig type '%s', radio address %02X%s", cn->serial,
	     radio_str(cn->radio), rtyp_get_name(rigtype), addr, addr ? "" : " (learn)");

	if(cn->monitor) {
		char line[64];
		cat_civ_set_frame_cb(cn->civ, catn_mon_frame_cb, cn);
		catn_mon_comment(cn, "rig type '%s'", rtyp_get_name(rigtype));
		cat_trace_civ_config(line, sizeof(line), &cc);
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

static void catn_keepalive_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	struct cat_native *cn = w->data;
	(void)loop; (void)revents;

	if(!cn->last_info_valid)
		return;
	dbg1("%s %s native CAT: no new data, sending the last state again", cn->serial, radio_str(cn->radio));
	mhc_update_radio_info(cn->ctl, CATN_SOURCE, &cn->last_info);
}

static void catn_feed(struct cat_native *cn, enum cat_dir dir, unsigned const char *data, int len) {
	catn_setup(cn, 0);
	if(!cn->civ || len <= 0)
		return;

	uint32_t applied = cat_civ_get_stats(cn->civ)->applied;
	uint64_t t_ms = (uint64_t)(ev_now(cn->loop) * 1000.0);
	cat_civ_feed(cn->civ, dir, data, len, t_ms);
	if(cat_civ_get_stats(cn->civ)->applied == applied)
		return;

	ev_timer *t = &cn->publish_timer;
	if(!ev_is_active(t) || ev_timer_remaining(cn->loop, t) > CATN_PUBLISH_DELAY) {
		ev_timer_stop(cn->loop, t);
		ev_timer_set(t, CATN_PUBLISH_DELAY, 0.);
		ev_timer_start(cn->loop, t);
	}
}

// Radio -> host, one byte per call.
static void catn_radio_cb(struct mh_router *router, unsigned const char *data, int len, int channel,
			  void *user_data) {
	(void)router; (void)channel;
	catn_feed(user_data, CAT_DIR_RADIO, data, len);
}

// Host -> radio, as the router reads it from the connectors.
static void catn_host_cb(struct mh_router *router, unsigned const char *data, int len, int channel,
			 void *user_data) {
	(void)router; (void)channel;
	catn_feed(user_data, CAT_DIR_HOST, data, len);
}

static void catn_keyer_state_cb(enum app_event_type type, const void *data, void *user_data) {
	struct cat_native *cn = user_data;
	const struct ev_keyer_state *ev = data;

	if(type != EV_KEYER_STATE || !ev || (ev->serial && strcmp(ev->serial, cn->serial)))
		return;

	// Values decoded before the keyer went away may be outdated: start over.
	catn_mon_comment(cn, "keyer %s, decoder starts over", mhc_state_str(ev->state));
	cat_civ_destroy(cn->civ);
	cn->civ = NULL;
	cn->rigtype = -1;
	cn->civ_addr = -1;
	catn_reset(cn);
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
	cn->ebus = app_ctx_get_eventbus(ctx);
	cn->monitor = cfg->monitor;
	if(cn->monitor)
		catn_mon_open(cn, cfg->monitor_dir ? cfg->monitor_dir : LOGDIR);
	cn->rigtype = -1;
	cn->civ_addr = -1;
	cat_state_init(&cn->st);

	ev_timer_init(&cn->publish_timer, catn_publish_cb, 0., 0.);
	cn->publish_timer.data = cn;
	ev_timer_init(&cn->keepalive_timer, catn_keepalive_cb, 0., CATN_KEEPALIVE);
	cn->keepalive_timer.data = cn;

	mhr_add_consumer_cb(cn->router, catn_radio_cb, cn->channel, cn);
	mhr_add_producer_tap_cb(cn->router, catn_host_cb, cn->channel, cn);
	cn->kscb = eventbus_subscribe(app_ctx_get_eventbus(ctx), EV_KEYER_STATE, catn_keyer_state_cb, cn);

	info("%s %s native CAT rig mode sync created (protocol %s%s)", cn->serial, radio_str(cn->radio),
	     cn->protocol == CAT_NATIVE_PROTO_CIV ? "civ" : "auto", cn->monitor ? ", monitor on" : "");
	catn_setup(cn, 1);
	return cn;
}

void cat_native_destroy(struct cat_native *cn) {
	if(!cn)
		return;

	if(cn->kscb)
		eventbus_unsubscribe(cn->kscb);
	mhr_rem_producer_tap_cb(cn->router, catn_host_cb, cn->channel, cn);
	mhr_rem_consumer_cb(cn->router, catn_radio_cb, cn->channel);
	ev_timer_stop(cn->loop, &cn->publish_timer);
	ev_timer_stop(cn->loop, &cn->keepalive_timer);
	cat_civ_destroy(cn->civ);
	if(cn->mon_file) {
		catn_mon_comment(cn, "monitor stopped");
		fclose(cn->mon_file);
	}
	free(cn->mon_path);
	info("%s %s native CAT rig mode sync destroyed", cn->serial, radio_str(cn->radio));
	free(cn->serial);
	free(cn);
}
