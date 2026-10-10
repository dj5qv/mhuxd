/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include "cat_trace.h"
#include "mhcontrol.h"

static const char *kind_str[] = {
	[CAT_FRAME_CMD] = "cmd",
	[CAT_FRAME_ECHO] = "echo",
	[CAT_FRAME_REPLY] = "reply",
	[CAT_FRAME_ACK] = "ack",
	[CAT_FRAME_NAK] = "nak",
	[CAT_FRAME_UNSOLICITED] = "unsolicited",
	[CAT_FRAME_UNMATCHED] = "unmatched",
	[CAT_FRAME_FOREIGN] = "foreign",
	[CAT_FRAME_BAD] = "bad",
	[CAT_FRAME_JAM] = "jam",
	[CAT_FRAME_DROPPED] = "dropped",
	[CAT_FRAME_ERROR] = "error",
};

const char *cat_frame_kind_str(enum cat_frame_kind kind) {
	if((unsigned)kind > CAT_FRAME_ERROR)
		return "?";
	return kind_str[kind];
}

const char *cat_trace_keyer_str(int keyer_mode) {
	switch(keyer_mode) {
	case MOD_CW:      return "CW";
	case MOD_VOICE:   return "VOICE";
	case MOD_FSK:     return "FSK";
	case MOD_DIGITAL: return "DIGITAL";
	default:          return "?";
	}
}

static void fmt_item(char *buf, size_t sz, const struct cat_item *it) {
	if(it->valid)
		snprintf(buf, sz, "%u", it->val);
	else
		snprintf(buf, sz, "?");
}

void cat_trace_state(char *buf, size_t sz, const struct cat_state *st) {
	char sel[16], unsel[16], split[16];
	enum cat_mode m = cat_state_mode(st, CAT_VFO_SEL);

	fmt_item(sel, sizeof(sel), &st->vfo[CAT_VFO_SEL].freq);
	fmt_item(unsel, sizeof(unsel), &st->vfo[CAT_VFO_UNSEL].freq);
	fmt_item(split, sizeof(split), &st->split);
	snprintf(buf, sz, "rx=%s unsel=%s mode=%s split=%s keyer=%s", sel, unsel,
		 m ? cat_mode_token(m) : "?", split, cat_trace_keyer_str(cat_mode_to_keyer(m)));
}

void cat_trace_frame(char *buf, size_t sz, const char *time, const struct cat_frame *f,
		     const struct cat_state *st) {
	char host_tag[16];
	const char *tag = f->dir == CAT_DIR_RADIO ? "R" : f->dir == CAT_DIR_OWN ? "M" : host_tag;
	size_t n = 0;

	snprintf(host_tag, sizeof(host_tag), "H%d", f->client > 0 ? f->client : 1);

#define APPEND(...) do { \
		if(n < sz) { \
			int r = snprintf(buf + n, sz - n, __VA_ARGS__); \
			n += r > 0 ? (size_t)r : 0; \
		} \
	} while(0)

	if(!sz)
		return;
	buf[0] = '\0';
	APPEND("%s %s", time, tag);
	if(f->text) {
		APPEND(" \"");
		for(size_t i = 0; i < f->len; i++) {
			uint8_t c = f->buf[i];
			if(c == '"' || c == '\\')
				APPEND("\\%c", c);
			else if(c >= 0x20 && c < 0x7f)
				APPEND("%c", c);
			else
				APPEND("\\x%02x", c);
		}
		APPEND("\"");
	} else {
		for(size_t i = 0; i < f->len; i++)
			APPEND(" %02x", f->buf[i]);
	}
	// Radio frames answering an own query (tag M) or a client's command name it: "# own reply",
	// "# H2 reply".
	APPEND("  # ");
	if(f->dir == CAT_DIR_RADIO && f->own)
		APPEND("own ");
	else if(f->dir == CAT_DIR_RADIO && f->client)
		APPEND("H%d ", f->client);
	APPEND("%s", cat_frame_kind_str(f->kind));
	if(f->applied && st) {
		char s[128];
		cat_trace_state(s, sizeof(s), st);
		APPEND(", applied: %s", s);
	}
#undef APPEND
}

void cat_trace_ascii_config(char *buf, size_t sz, const struct cat_ascii_cfg *cfg) {
	snprintf(buf, sz, "@%s%s", cat_ascii_dialect_str(cfg->dialect), cfg->no_data ? " nodata" : "");
}

void cat_trace_ybin_config(char *buf, size_t sz, const struct cat_ybin_cfg *cfg) {
	snprintf(buf, sz, "@ybin model=%s", cat_ybin_model_str(cfg->model));
}

void cat_trace_civ_config(char *buf, size_t sz, const struct cat_civ_cfg *cfg) {
	if(cfg->freq_len && cfg->freq_len != 5)
		snprintf(buf, sz, "@civ addr=%02x freq_len=%u", cfg->radio_addr, cfg->freq_len);
	else
		snprintf(buf, sz, "@civ addr=%02x", cfg->radio_addr);
}
