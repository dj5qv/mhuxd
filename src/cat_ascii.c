/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Command layouts: Kenwood TS-590 PC command reference, Hamlib backends
 * (rigs/kenwood/kenwood.c, k3.c, flex6xxx.c, rigs/yaesu/newcat.c).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cat_ascii.h"

#define PEND_MAX 8
#define PEND_TIMEOUT_MS 2000
#define PREFIX_LEN 5		// "ZZFA" + NUL

#define IF_LEN_KENWOOD 37	// without ';'

enum { VFO_A, VFO_B };

struct ascii_rx {
	uint8_t buf[ASCII_MAX_FRAME];
	size_t len;
};

struct ascii_pending {
	char prefix[PREFIX_LEN];
	uint64_t t_ms;
};

struct cat_ascii {
	struct cat_ascii_cfg cfg;
	struct cat_state *st;
	cat_frame_cb cb;
	void *cb_data;
	struct ascii_rx rx[CAT_DIR_RADIO + 1];
	// Client commands, only to tell replies from auto information.
	struct ascii_pending pend[PEND_MAX];
	int npend;
	// Elecraft: mode code and data sub mode (DT) per VFO, 0 = unknown.
	char k3_md[2];
	char k3_dt[2];
	// TX VFO from FT, -1 = unknown.
	int tx_ab;
	struct cat_stats stats;
};

static const char *dialect_names[] = {
	[CAT_ASCII_KENWOOD] = "kenwood",
	[CAT_ASCII_ELECRAFT] = "elecraft",
	[CAT_ASCII_YAESU] = "yaesu",
	[CAT_ASCII_FLEX] = "flex",
};

const char *cat_ascii_dialect_str(enum cat_ascii_dialect dialect) {
	if((unsigned)dialect > CAT_ASCII_FLEX)
		return "?";
	return dialect_names[dialect];
}

int cat_ascii_dialect_from_str(const char *s) {
	for(int i = 0; s && i <= CAT_ASCII_FLEX; i++)
		if(!strcmp(s, dialect_names[i]))
			return i;
	return -1;
}

struct cat_ascii *cat_ascii_create(const struct cat_ascii_cfg *cfg, struct cat_state *st) {
	struct cat_ascii *a = calloc(1, sizeof(*a));
	if(!a)
		return NULL;
	if(cfg)
		a->cfg = *cfg;
	a->st = st;
	a->tx_ab = -1;
	return a;
}

void cat_ascii_destroy(struct cat_ascii *a) {
	free(a);
}

void cat_ascii_set_frame_cb(struct cat_ascii *a, cat_frame_cb cb, void *user_data) {
	a->cb = cb;
	a->cb_data = user_data;
}

const struct cat_stats *cat_ascii_get_stats(const struct cat_ascii *a) {
	return &a->stats;
}

static void emit(struct cat_ascii *a, enum cat_frame_kind kind, enum cat_dir dir,
		 const uint8_t *buf, size_t len, int applied, uint64_t t_ms) {
	switch(kind) {
	case CAT_FRAME_REPLY:   a->stats.replies++; break;
	case CAT_FRAME_BAD:     a->stats.bad++; break;
	case CAT_FRAME_DROPPED: a->stats.dropped++; break;
	case CAT_FRAME_ERROR:   a->stats.errors++; break;
	default: break;
	}
	if(applied)
		a->stats.applied++;

	if(a->cb) {
		struct cat_frame f = {
			.kind = kind, .dir = dir, .t_ms = t_ms,
			.buf = buf, .len = len, .applied = applied, .text = 1,
		};
		a->cb(&f, a->cb_data);
	}
}

/*
 * Field decoding
 */

// n digits at p, as frequency in Hz.
static int dec_freq(const char *p, size_t n, uint32_t *hz) {
	uint64_t f = 0;
	if(n < 8 || n > 11)
		return -1;
	for(size_t i = 0; i < n; i++) {
		if(p[i] < '0' || p[i] > '9')
			return -1;
		f = f * 10 + (p[i] - '0');
	}
	if(f == 0 || f > UINT32_MAX)
		return -1;
	*hz = (uint32_t)f;
	return 0;
}

static int dec_bool(char c, int *v) {
	if(c != '0' && c != '1')
		return -1;
	*v = c - '0';
	return 0;
}

// Mode code of MD, IF, OM, SF. *data: 1/0 if the code tells, -1 if a data flag command (DA)
// does. data_codes: the command has codes of its own for the data modes (OM, SF), so the
// others mean data off.
static int dec_mode_kenwood(const struct cat_ascii *a, char c, int data_codes, enum cat_mode *mode,
			    int *data) {
	*data = a->cfg.no_data || data_codes ? 0 : -1;
	switch(c) {
	case '1': *mode = CAT_MODE_LSB; break;
	case '2': *mode = CAT_MODE_USB; break;
	case '3': *mode = CAT_MODE_CW; break;
	case '4': *mode = CAT_MODE_FM; break;
	case '5': *mode = CAT_MODE_AM; break;
	case '6': *mode = CAT_MODE_RTTY; break;		// FSK
	case '7': *mode = CAT_MODE_CWR; break;
	case '9': *mode = CAT_MODE_RTTYR; break;	// FSK-R
	// TS-990/TS-890: PSK and the data modes have codes of their own.
	case 'A': *mode = CAT_MODE_PSK; *data = 0; break;
	case 'B': *mode = CAT_MODE_PSKR; *data = 0; break;
	case 'C': *mode = CAT_MODE_PKTLSB; *data = 1; break;
	case 'D': *mode = CAT_MODE_PKTUSB; *data = 1; break;
	case 'E': *mode = CAT_MODE_PKTFM; *data = 1; break;
	case 'F': *mode = CAT_MODE_PKTAM; *data = 1; break;
	default: return -1;	// 0, 8: none / setting failure
	}
	return 0;
}

// Flex and PowerSDR in Kenwood emulation use FSK/FSK-R for DIGL/DIGU.
static int dec_mode_flex(char c, enum cat_mode *mode, int *data) {
	*data = 0;
	switch(c) {
	case '1': *mode = CAT_MODE_LSB; break;
	case '2': *mode = CAT_MODE_USB; break;
	case '3': *mode = CAT_MODE_CW; break;
	case '4': *mode = CAT_MODE_FM; break;
	case '5': *mode = CAT_MODE_AM; break;
	case '6': *mode = CAT_MODE_PKTLSB; *data = 1; break;
	case '7': *mode = CAT_MODE_CWR; break;
	case '9': *mode = CAT_MODE_PKTUSB; *data = 1; break;
	default: return -1;
	}
	return 0;
}

// PowerSDR ZZMD/ZZME, two digits (one is accepted too, Hamlib's simulator sends that).
static int dec_mode_zz(const char *p, size_t n, enum cat_mode *mode, int *data) {
	int v = 0;
	if(n < 1 || n > 2)
		return -1;
	for(size_t i = 0; i < n; i++) {
		if(p[i] < '0' || p[i] > '9')
			return -1;
		v = v * 10 + p[i] - '0';
	}
	*data = 0;
	switch(v) {
	case 0:  *mode = CAT_MODE_LSB; break;
	case 1:  *mode = CAT_MODE_USB; break;
	case 3:  *mode = CAT_MODE_CWR; break;		// CWL
	case 4:  *mode = CAT_MODE_CW; break;		// CWU
	case 5:  *mode = CAT_MODE_FM; break;
	case 6:  *mode = CAT_MODE_AM; break;
	case 7:  *mode = CAT_MODE_PKTUSB; *data = 1; break;	// DIGU
	case 9:  *mode = CAT_MODE_PKTLSB; *data = 1; break;	// DIGL
	case 10: *mode = CAT_MODE_AMS; break;		// SAM
	case 2: case 8: case 11:			// DSB, SPEC, DRM
		*mode = CAT_MODE_OTHER; break;
	default: return -1;
	}
	return 0;
}

// Yaesu newcat: the data modes have codes of their own.
static int dec_mode_yaesu(char c, enum cat_mode *mode, int *data) {
	*data = 0;
	switch(c) {
	case '1': *mode = CAT_MODE_LSB; break;
	case '2': *mode = CAT_MODE_USB; break;
	case '3': *mode = CAT_MODE_CW; break;		// CW-U
	case '4': *mode = CAT_MODE_FM; break;
	case '5': *mode = CAT_MODE_AM; break;
	case '6': *mode = CAT_MODE_RTTY; break;		// RTTY-LSB
	case '7': *mode = CAT_MODE_CWR; break;		// CW-L
	case '8': *mode = CAT_MODE_PKTLSB; *data = 1; break;	// DATA-LSB
	case '9': *mode = CAT_MODE_RTTYR; break;	// RTTY-USB
	case 'A': *mode = CAT_MODE_PKTFM; *data = 1; break;	// DATA-FM
	case 'B': *mode = CAT_MODE_FM; break;		// FM-N
	case 'C': *mode = CAT_MODE_PKTUSB; *data = 1; break;	// DATA-USB
	case 'D': *mode = CAT_MODE_AM; break;		// AM-N
	case 'E': *mode = CAT_MODE_OTHER; break;	// C4FM
	case 'F': *mode = CAT_MODE_PKTFM; *data = 1; break;	// DATA-FM-N
	default: return -1;
	}
	return 0;
}

// Elecraft K3: DATA / DATA-REV (MD6/MD9) with the sub mode from DT. Without a DT
// value yet, DATA A (the default, used for the sound card modes) is assumed.
static enum cat_mode k3_data_mode(char md, char dt) {
	switch(dt) {
	case '1': return CAT_MODE_PKTLSB;			// AFSK A, as Hamlib does
	case '2': return md == '9' ? CAT_MODE_RTTYR : CAT_MODE_RTTY;	// FSK D
	default:  return CAT_MODE_PKTUSB;			// DATA A, PSK D
	}
}

/*
 * State updates. VFOs are addressed as A/B here; cat_state keeps the selected
 * (receive) VFO and the other one.
 */

static int slot_of(const struct cat_ascii *a, int ab) {
	return (uint32_t)ab == a->st->vfo_ab.val ? CAT_VFO_SEL : CAT_VFO_UNSEL;
}

static void set_freq(struct cat_ascii *a, int ab, uint32_t hz, uint64_t t) {
	cat_state_set_freq(a->st, slot_of(a, ab), hz, t);
}

// data: 0/1 if the mode code tells, -1 if a data flag command (DA) does.
static void set_mode(struct cat_ascii *a, int slot, enum cat_mode mode, int data, uint64_t t) {
	cat_state_set_mode(a->st, slot, mode, t);	// the PKT modes set the data flag themselves
	if(data == 0 && cat_mode_with_data(mode, 1) != mode)
		cat_state_set_data(a->st, slot, 0, t);
}

static void update_split(struct cat_ascii *a, uint64_t t) {
	if(a->tx_ab >= 0)
		cat_state_set_split(a->st, (uint32_t)a->tx_ab != a->st->vfo_ab.val, t);
}

// Split reported as such (IF, ST, ZZSP): the TX VFO follows from it.
static void set_split(struct cat_ascii *a, int split, uint64_t t) {
	cat_state_set_split(a->st, split, t);
	a->tx_ab = split ? !a->st->vfo_ab.val : (int)a->st->vfo_ab.val;
}

static void select_rx(struct cat_ascii *a, int ab, uint64_t t) {
	cat_state_select_vfo(a->st, ab, t);
	update_split(a, t);
}

static void k3_apply(struct cat_ascii *a, int ab, uint64_t t) {
	char md = a->k3_md[ab];
	enum cat_mode mode;
	int data;

	if(!md)
		return;
	if(md == '6' || md == '9') {
		set_mode(a, slot_of(a, ab), k3_data_mode(md, a->k3_dt[ab]), 1, t);
	} else if(dec_mode_kenwood(a, md, 0, &mode, &data) == 0) {
		set_mode(a, slot_of(a, ab), mode, 0, t);	// K3: data only in the DATA modes
	}
}

// Mode code for VFO ab, any dialect. data_codes: see dec_mode_kenwood().
static int apply_mode_code(struct cat_ascii *a, int ab, char c, int data_codes, uint64_t t) {
	enum cat_mode mode;
	int data;

	switch(a->cfg.dialect) {
	case CAT_ASCII_ELECRAFT:
		if(c < '1' || c > '9' || c == '8')
			return -1;
		a->k3_md[ab] = c;
		k3_apply(a, ab, t);
		return 1;
	case CAT_ASCII_FLEX:
		if(dec_mode_flex(c, &mode, &data))
			return -1;
		break;
	case CAT_ASCII_YAESU:
		if(dec_mode_yaesu(c, &mode, &data))
			return -1;
		break;
	default:
		if(dec_mode_kenwood(a, c, data_codes, &mode, &data))
			return -1;
		break;
	}
	set_mode(a, slot_of(a, ab), mode, data, t);
	return 1;
}

/*
 * Commands. s: frame without ';', n: its length. Return 1 if the state was
 * updated, 0 if the frame carries nothing of interest (or is a query), -1 if
 * the layout or values don't fit.
 */

static int cmd_freq(struct cat_ascii *a, int ab, const char *p, size_t n, uint64_t t) {
	uint32_t hz;
	if(n == 0)
		return 0;	// query
	if(dec_freq(p, n, &hz))
		return -1;
	set_freq(a, ab, hz, t);
	return 1;
}

// Kenwood, Elecraft, Flex: IF + 35 characters.
static int cmd_if_kenwood(struct cat_ascii *a, const char *s, size_t n, uint64_t t) {
	uint32_t hz;
	int tx, split;

	if(n == 2)
		return 0;
	if(n != IF_LEN_KENWOOD || dec_freq(s + 2, 11, &hz) || dec_bool(s[28], &tx) ||
	   dec_bool(s[32], &split) || s[30] < '0' || s[30] > '2')
		return -1;

	// Transmitting in split, IF shows the TX VFO (Hamlib, kenwood.c).
	if(!(tx && split)) {
		if(s[30] != '2')	// 2: memory channel
			cat_state_select_vfo(a->st, s[30] - '0', t);
		set_freq(a, a->st->vfo_ab.val, hz, t);
		if(apply_mode_code(a, a->st->vfo_ab.val, s[29], 0, t) < 0)
			return -1;
	}
	set_split(a, split, t);
	return 1;
}

// Yaesu: IF (VFO A) / OI (VFO B): memory channel (3), frequency (8 or 9), clarifier (5),
// RX clarifier, TX clarifier, mode, VFO/memory, ...
static int cmd_if_yaesu(struct cat_ascii *a, int ab, const char *s, size_t n, uint64_t t) {
	size_t w;
	uint32_t hz;

	switch(n) {
	case 2:  return 0;
	case 26: case 29: w = 8; break;
	case 27: case 40: w = 9; break;
	default: return -1;
	}
	if(dec_freq(s + 5, w, &hz))
		return -1;
	set_freq(a, ab, hz, t);
	if(apply_mode_code(a, ab, s[5 + w + 7], 0, t) < 0)
		return -1;
	return 1;
}

static int cmd_md(struct cat_ascii *a, const char *s, size_t n, uint64_t t) {
	int ab = a->st->vfo_ab.val;

	if(a->cfg.dialect == CAT_ASCII_YAESU) {
		// MD0x main / VFO A, MD1x sub / VFO B; MD0 is the query.
		if(n == 3 || n == 2)
			return 0;
		if(n != 4 || (s[2] != '0' && s[2] != '1'))
			return -1;
		return apply_mode_code(a, s[2] - '0', s[3], 0, t);
	}
	if(a->cfg.dialect == CAT_ASCII_ELECRAFT && n >= 3 && s[2] == '$') {
		if(n == 3)
			return 0;
		return n == 4 ? apply_mode_code(a, VFO_B, s[3], 0, t) : -1;
	}
	if(n == 2)
		return 0;
	return n == 3 ? apply_mode_code(a, ab, s[2], 0, t) : -1;
}

// Kenwood DA (TS-590, TS-990), DT (TS-950): data mode of the current VFO.
// Elecraft DT: data sub mode, DT$ of VFO B.
static int cmd_data(struct cat_ascii *a, const char *s, size_t n, uint64_t t) {
	int v;

	if(a->cfg.dialect == CAT_ASCII_ELECRAFT) {
		int vfo_b = n >= 3 && s[2] == '$';
		size_t p = vfo_b ? 3 : 2;	// position of the value
		int ab = vfo_b ? VFO_B : (int)a->st->vfo_ab.val;
		if(s[1] != 'T' || n == p)
			return 0;
		if(n != p + 1 || s[p] < '0' || s[p] > '3')
			return -1;
		a->k3_dt[ab] = s[p];
		k3_apply(a, ab, t);
		return 1;
	}
	if(a->cfg.dialect != CAT_ASCII_KENWOOD)
		return 0;
	if(n == 2)
		return 0;
	if(n != 3 || dec_bool(s[2], &v))
		return -1;
	cat_state_set_data(a->st, CAT_VFO_SEL, v, t);
	return 1;
}

// FR: receive VFO (Kenwood), FT: transmit VFO (Kenwood, Yaesu), VS: selected VFO (Yaesu).
static int cmd_vfo(struct cat_ascii *a, const char *s, size_t n, uint64_t t) {
	if(n == 2)
		return 0;
	if(n != 3 || s[2] < '0' || s[2] > '3')
		return -1;
	if(s[2] > '1')
		return 0;	// FR2: memory, FT2/FT3 (FTdx101): set only

	if(!strncmp(s, "FT", 2)) {
		a->tx_ab = s[2] - '0';
		update_split(a, t);
	} else {
		select_rx(a, s[2] - '0', t);
	}
	return 1;
}

// Yaesu ST: split. Flex/PowerSDR ZZSP: split.
static int cmd_split(struct cat_ascii *a, char c, uint64_t t) {
	if(c < '0' || c > '2')
		return -1;
	set_split(a, c != '0', t);	// ST2: split +5 kHz
	return 1;
}

// TS-990 OM0x / OM1x: mode of main / sub band.
static int cmd_om(struct cat_ascii *a, const char *s, size_t n, uint64_t t) {
	if(n == 3 || n == 2)
		return 0;
	if(n != 4 || (s[2] != '0' && s[2] != '1'))
		return -1;
	return apply_mode_code(a, s[2] - '0', s[3], 1, t);
}

// TS-890 SF0 / SF1: frequency and mode of VFO A / B.
static int cmd_sf(struct cat_ascii *a, const char *s, size_t n, uint64_t t) {
	uint32_t hz;
	if(n == 3 || n == 2)
		return 0;
	if(n != 15 || (s[2] != '0' && s[2] != '1') || dec_freq(s + 3, 11, &hz))
		return -1;
	set_freq(a, s[2] - '0', hz, t);
	return apply_mode_code(a, s[2] - '0', s[14], 1, t);
}

static int cmd_zz(struct cat_ascii *a, const char *s, size_t n, uint64_t t) {
	enum cat_mode mode;
	int data, ab;

	if(n == 4)
		return 0;	// query
	if(!strncmp(s, "ZZFA", 4) || !strncmp(s, "ZZFB", 4))
		return cmd_freq(a, s[3] == 'A' ? VFO_A : VFO_B, s + 4, n - 4, t);
	if(!strncmp(s, "ZZMD", 4) || !strncmp(s, "ZZME", 4)) {
		ab = s[3] == 'D' ? VFO_A : VFO_B;
		if(dec_mode_zz(s + 4, n - 4, &mode, &data))
			return -1;
		set_mode(a, slot_of(a, ab), mode, data, t);
		return 1;
	}
	if(!strncmp(s, "ZZSP", 4))
		return n == 5 ? cmd_split(a, s[4], t) : -1;
	return 0;
}

// set: frame from the client. Only frequency, mode and data mode are taken from
// client frames; VFO selection and split come from the radio's answers.
static int apply_frame(struct cat_ascii *a, const char *s, size_t n, int set, uint64_t t) {
	int yaesu = a->cfg.dialect == CAT_ASCII_YAESU;

	if(n < 2)
		return 0;
	if(!strncmp(s, "FA", 2) || !strncmp(s, "FB", 2))
		return cmd_freq(a, s[1] == 'A' ? VFO_A : VFO_B, s + 2, n - 2, t);
	if(!strncmp(s, "MD", 2))
		return cmd_md(a, s, n, t);
	if(!strncmp(s, "DA", 2) || !strncmp(s, "DT", 2))
		return cmd_data(a, s, n, t);
	if(a->cfg.dialect == CAT_ASCII_FLEX && n >= 4 && !strncmp(s, "ZZ", 2))
		return set && !strncmp(s, "ZZSP", 4) ? 0 : cmd_zz(a, s, n, t);
	if(set)
		return 0;

	if(!strncmp(s, "IF", 2))
		return yaesu ? cmd_if_yaesu(a, VFO_A, s, n, t) : cmd_if_kenwood(a, s, n, t);
	if(yaesu && !strncmp(s, "OI", 2))
		return cmd_if_yaesu(a, VFO_B, s, n, t);
	if(!strncmp(s, "FT", 2) || (!yaesu && !strncmp(s, "FR", 2)) || (yaesu && !strncmp(s, "VS", 2)))
		return cmd_vfo(a, s, n, t);
	if(yaesu && !strncmp(s, "ST", 2))
		return n == 2 ? 0 : n == 3 ? cmd_split(a, s[2], t) : -1;
	if(a->cfg.dialect == CAT_ASCII_KENWOOD && !strncmp(s, "OM", 2))
		return cmd_om(a, s, n, t);
	if(a->cfg.dialect == CAT_ASCII_KENWOOD && !strncmp(s, "SF", 2))
		return cmd_sf(a, s, n, t);
	return 0;
}

/*
 * Pending client commands
 */

static void prefix_of(const char *s, size_t n, char *prefix) {
	size_t len = (n >= 4 && !strncmp(s, "ZZ", 2)) ? 4 : n >= 2 ? 2 : n;
	memcpy(prefix, s, len);
	prefix[len] = '\0';
}

static void pend_remove(struct cat_ascii *a, int i) {
	memmove(&a->pend[i], &a->pend[i + 1], (a->npend - i - 1) * sizeof(a->pend[0]));
	a->npend--;
}

static void pend_add(struct cat_ascii *a, const char *prefix, uint64_t t) {
	while(a->npend && a->pend[0].t_ms + PEND_TIMEOUT_MS < t)
		pend_remove(a, 0);
	if(a->npend == PEND_MAX)
		pend_remove(a, 0);
	snprintf(a->pend[a->npend].prefix, PREFIX_LEN, "%s", prefix);
	a->pend[a->npend].t_ms = t;
	a->npend++;
}

// The reply to the oldest pending command with this prefix, any if prefix is NULL (error).
static int pend_take(struct cat_ascii *a, const char *prefix, uint64_t t) {
	for(int i = 0; i < a->npend; i++) {
		if(a->pend[i].t_ms + PEND_TIMEOUT_MS < t)
			continue;
		if(!prefix || !strcmp(a->pend[i].prefix, prefix)) {
			pend_remove(a, i);
			return 1;
		}
	}
	return 0;
}

static void process_frame(struct cat_ascii *a, enum cat_dir dir, const uint8_t *buf, size_t len, uint64_t t) {
	const char *s = (const char *)buf;
	size_t n = len - 1;	// without ';'
	char prefix[PREFIX_LEN];
	int r;

	a->stats.frames++;
	if(!a->st->vfo_ab.valid)
		cat_state_select_vfo(a->st, VFO_A, t);	// until the radio tells otherwise

	prefix_of(s, n, prefix);

	if(dir != CAT_DIR_RADIO) {
		if(n)
			pend_add(a, prefix, t);
		r = apply_frame(a, s, n, 1, t);
		emit(a, CAT_FRAME_CMD, dir, buf, len, r > 0, t);
		return;
	}

	if(n == 1 && (s[0] == '?' || s[0] == 'E' || s[0] == 'O')) {
		pend_take(a, NULL, t);
		emit(a, CAT_FRAME_ERROR, dir, buf, len, 0, t);
		return;
	}

	int reply = n && pend_take(a, prefix, t);
	r = apply_frame(a, s, n, 0, t);
	emit(a, r < 0 ? CAT_FRAME_BAD : reply ? CAT_FRAME_REPLY : CAT_FRAME_UNSOLICITED,
	     dir, buf, len, r > 0, t);
}

/*
 * Framer
 */

static void rx_byte(struct cat_ascii *a, enum cat_dir dir, uint8_t b, uint64_t t) {
	struct ascii_rx *rx = &a->rx[dir];

	if(b == ';') {
		rx->buf[rx->len++] = b;
		process_frame(a, dir, rx->buf, rx->len, t);
		rx->len = 0;
		return;
	}

	// Line ends and spaces between frames (some clients add CR/LF).
	if(!rx->len && (b == '\r' || b == '\n' || b == ' ' || b == '\0'))
		return;

	// Binary data, or no room left for the ';': drop what we have and start over. There is
	// no start marker to wait for; the rest of a broken frame ends up as an unknown command,
	// and every byte stays in the CAT monitor, so its traces replay the same way.
	if(b < 0x20 || b >= 0x7f || rx->len + 1 >= ASCII_MAX_FRAME) {
		rx->buf[rx->len++] = b;
		emit(a, CAT_FRAME_DROPPED, dir, rx->buf, rx->len, 0, t);
		rx->len = 0;
		return;
	}
	rx->buf[rx->len++] = b;
}

void cat_ascii_feed(struct cat_ascii *a, enum cat_dir dir, const uint8_t *buf, size_t len, uint64_t t_ms) {
	if(dir > CAT_DIR_RADIO)
		return;
	for(size_t i = 0; i < len; i++)
		rx_byte(a, dir, buf[i], t_ms);
}
