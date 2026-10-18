/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Answer lengths and data layouts: MARK-V FT-1000MP operating manual (CAT
 * chapter), Hamlib backends (rigs/yaesu/ft757gx.c, ft817.c, ft847.c, ft840.c,
 * ft890.c, ft900.c, ft920.c, ft990.c, ft1000d.c, ft1000mp.c).
 */

#include <stdlib.h>
#include <string.h>
#include "cat_ybin.h"

#define PEND_MAX 8
// A command still waiting for its first answer byte when another command is sent this much
// later was not answered (radio busy, CAT off, unknown command): the next answer belongs to
// the newer command.
#define GIVEUP_MS 200
// Answer length flag: the length is a guess (memory dumps differ between firmware versions),
// the answer may end earlier.
#define VAR 0x8000
#define LEN_MASK 0x7fff

enum family {
	FAM_757,	// 0x10: 75 bytes, BCD frequencies
	FAM_817,	// FT-817, FT-857, FT-897: 0x03 frequency and mode, every command answered
	FAM_847,	// like FT-817 without acknowledge
	FAM_900,	// FT-840, FT-890, FT-900: 0x10 records of 18/19 bytes
	FAM_920,	// 0x10 records of 14 bytes, frequency in Hz
	FAM_990,	// 0x10 records of 16 bytes, current data 32 bytes
	FAM_1000,	// FT-1000, FT-1000D: 0x10 records of 16 bytes
	FAM_MP,		// FT-1000MP: 0x10 records of 16 bytes, frequency in 0.625 Hz
};

static const struct ybin_model {
	const char *id;
	const char *name;
	uint8_t fam;
	uint16_t dump_len;	// 0x10 with all data, 0: unknown
	uint16_t hold_ms;	// a command without answer keeps the line this long, 0: YBIN_HOLD_MS
} models[CAT_YBIN_MODEL_NUM] = {
	// No command buffer, a command takes 150 ms (OmniRig's FT-757GXII.ini).
	[CAT_YBIN_FT757GX2] = { "ft757gx2", "FT-757GXII", FAM_757, 0, 150 },
	[CAT_YBIN_FT817] = { "ft817", "FT-817", FAM_817, 0 },
	[CAT_YBIN_FT857] = { "ft857", "FT-857", FAM_817, 0 },
	[CAT_YBIN_FT897] = { "ft897", "FT-897", FAM_817, 0 },
	[CAT_YBIN_FT847] = { "ft847", "FT-847", FAM_847, 0 },
	[CAT_YBIN_FT840] = { "ft840", "FT-840", FAM_900, 649 },
	[CAT_YBIN_FT890] = { "ft890", "FT-890", FAM_900, 649 },
	[CAT_YBIN_FT900] = { "ft900", "FT-900", FAM_900, 1941 },
	[CAT_YBIN_FT920] = { "ft920", "FT-920", FAM_920, 0 },
	[CAT_YBIN_FT990] = { "ft990", "FT-990", FAM_990, 1508 },	// ROM 1.2: 1492
	[CAT_YBIN_FT1000] = { "ft1000", "FT-1000", FAM_1000, 1636 },
	[CAT_YBIN_FT1000MP] = { "ft1000mp", "FT-1000MP", FAM_MP, 1863 },
};

enum { VFO_A, VFO_B };

struct ybin_rx {
	uint8_t buf[YBIN_CMD_LEN];
	size_t len;
	uint64_t t_ms;		// last byte
};

// A framer per source: one per client (index client - 1), mhuxd's own queries.
#define RX_OWN CAT_MAX_CLIENTS
#define RX_NUM (CAT_MAX_CLIENTS + 1)

struct ybin_pending {
	uint8_t cmd[YBIN_CMD_LEN];
	uint16_t len;		// answer length
	uint8_t var;		// the answer may be shorter
	uint8_t own;		// mhuxd's own query
	uint8_t client;
	uint16_t got;		// answer bytes so far
	uint64_t t_ms;		// sent
	uint64_t t_last;	// last answer byte
};

struct cat_ybin {
	struct cat_ybin_cfg cfg;
	const struct ybin_model *m;
	struct cat_state *st;
	cat_frame_cb cb;
	void *cb_data;
	struct ybin_rx rx[RX_NUM];
	// Commands waiting for their answer, oldest first: the radio answers in order.
	struct ybin_pending pend[PEND_MAX];
	int npend;
	// The answer of pend[0] being received, the current piece of a long one.
	uint8_t ans[YBIN_MAX_FRAME];
	size_t ans_len;
	// Commands without answer keep the line until then, index 0: mhuxd's own.
	uint64_t hold[CAT_MAX_CLIENTS + 1];
	unsigned pacing_ms;	// delay between the answer bytes (command 0x0e)
	struct cat_stats stats;
};

const char *cat_ybin_model_str(enum cat_ybin_model model) {
	if((unsigned)model >= CAT_YBIN_MODEL_NUM)
		return "?";
	return models[model].id;
}

const char *cat_ybin_model_name(enum cat_ybin_model model) {
	if((unsigned)model >= CAT_YBIN_MODEL_NUM)
		return "?";
	return models[model].name;
}

int cat_ybin_model_from_str(const char *s) {
	for(int i = 0; s && i < CAT_YBIN_MODEL_NUM; i++)
		if(!strcmp(s, models[i].id))
			return i;
	return -1;
}

struct cat_ybin *cat_ybin_create(const struct cat_ybin_cfg *cfg, struct cat_state *st) {
	if(!cfg || (unsigned)cfg->model >= CAT_YBIN_MODEL_NUM)
		return NULL;
	struct cat_ybin *y = calloc(1, sizeof(*y));
	if(!y)
		return NULL;
	y->cfg = *cfg;
	y->m = &models[cfg->model];
	y->st = st;
	return y;
}

void cat_ybin_destroy(struct cat_ybin *y) {
	free(y);
}

void cat_ybin_set_frame_cb(struct cat_ybin *y, cat_frame_cb cb, void *user_data) {
	y->cb = cb;
	y->cb_data = user_data;
}

const struct cat_stats *cat_ybin_get_stats(const struct cat_ybin *y) {
	return &y->stats;
}

static void emit(struct cat_ybin *y, enum cat_frame_kind kind, enum cat_dir dir, const uint8_t *buf,
		 size_t len, int applied, int own, int client, uint64_t t_ms) {
	y->stats.frames++;
	switch(kind) {
	case CAT_FRAME_REPLY:     y->stats.replies++; break;
	case CAT_FRAME_BAD:       y->stats.bad++; break;
	case CAT_FRAME_DROPPED:   y->stats.dropped++; break;
	case CAT_FRAME_UNMATCHED: y->stats.unmatched++; break;
	default: break;
	}
	if(applied)
		y->stats.applied++;
	if(own && dir == CAT_DIR_RADIO)
		y->stats.own++;

	if(y->cb) {
		struct cat_frame f = {
			.kind = kind, .dir = dir, .t_ms = t_ms,
			.buf = buf, .len = len, .applied = applied, .own = own, .client = client,
		};
		y->cb(&f, y->cb_data);
	}
}

/*
 * Answer lengths
 */

// Bytes the radio answers the command with, | VAR if that's a guess; 0: no answer.
static unsigned answer_len(const struct cat_ybin *y, const uint8_t *c) {
	uint8_t op = c[4], u = c[3];
	unsigned dump = y->m->dump_len ? y->m->dump_len | VAR : LEN_MASK | VAR;

	switch(y->m->fam) {
	case FAM_757:
		// Status update: all, or (U = 1) the meter only. Setting the return delay (0x0e)
		// returns the status data as well.
		if(op == 0x10)
			return u == 0x01 ? 1 : 75;
		return op == 0x0e ? 75 : 0;
	case FAM_817:
		switch(op) {
		case 0x03: return 5;		// frequency and mode
		case 0xbb: return 2;		// EEPROM read
		case 0xbd: return 2;		// TX meters
		case 0xff: return 0;		// wake-up sequence (FF FF FF FF FF)
		default:   return 1;		// status byte (E7, F7) or acknowledge
		}
	case FAM_847:
		switch(op) {
		case 0x03: case 0x13: case 0x23: return 5;	// main, satellite RX, satellite TX
		case 0xe7: case 0xf7: return 1;
		default: return 0;
		}
	default:
		break;
	}

	if(op == 0x10) {
		static const uint16_t lens[][5] = {
			//           all  mem   current  A+B  memory
			[FAM_900]  = { 0, 1,      19,    18,  19 },
			[FAM_920]  = { 0, 1,      28,    28,  14 },
			[FAM_990]  = { 0, 1,      32,    32,  16 },
			[FAM_1000] = { 0, 1,      16,    32,  16 },
			[FAM_MP]   = { 0, 1,      16,    32,  16 },
		};
		if(u > 4)
			return 0;
		return u == 0 ? dump : lens[y->m->fam][u];
	}
	if(op == 0xfa) {
		switch(y->m->fam) {
		case FAM_MP:  return u & 1 ? 6 : 5;	// 6 flag bytes, or 3 and 2 model ID bytes
		case FAM_920: return 8 | (u == 1 ? 0 : VAR);
		default:      return 5;
		}
	}
	if(op == 0xf7 && y->m->fam != FAM_920)
		return 5;	// meter
	return 0;
}

// Command 0x0e sets a delay between the answer bytes.
static int has_pacing(const struct cat_ybin *y) {
	return y->m->fam != FAM_817 && y->m->fam != FAM_847;
}

/*
 * Field decoding
 */

// n packed BCD bytes, most significant first (be) or last, as an integer.
static int dec_bcd(const uint8_t *p, int n, int be, uint32_t *v) {
	uint32_t r = 0;
	for(int i = 0; i < n; i++) {
		uint8_t b = p[be ? i : n - 1 - i];
		if((b >> 4) > 9 || (b & 0x0f) > 9)
			return -1;
		r = r * 100 + (b >> 4) * 10 + (b & 0x0f);
	}
	*v = r;
	return 0;
}

static uint32_t dec_be(const uint8_t *p, int n) {
	uint32_t r = 0;
	for(int i = 0; i < n; i++)
		r = r << 8 | p[i];
	return r;
}

// FT-817, FT-857, FT-897 (0x03 answer, 0x07 parameter); FT-847 has the same codes up to 0x08.
// Bit 7: narrow filter.
static enum cat_mode mode_817(uint8_t c) {
	switch(c & 0x7f) {
	case 0x00: return CAT_MODE_LSB;
	case 0x01: return CAT_MODE_USB;
	case 0x02: return CAT_MODE_CW;
	case 0x03: return CAT_MODE_CWR;
	case 0x04: return CAT_MODE_AM;
	case 0x06: return CAT_MODE_WFM;
	case 0x08: return CAT_MODE_FM;
	case 0x0a: return CAT_MODE_PKTUSB;	// DIG: RTTY, PSK31 or USER-L/U as set in the menu
	case 0x0c: return CAT_MODE_PKTFM;	// PKT
	default:   return CAT_MODE_NONE;
	}
}

// Mode set parameter (0x0c) of the FT-1000MP, FT-1000, FT-990: bit 7 selects VFO B.
static enum cat_mode modeset_1000(uint8_t c) {
	switch(c & 0x7f) {
	case 0x00: return CAT_MODE_LSB;
	case 0x01: return CAT_MODE_USB;
	case 0x02: return CAT_MODE_CW;
	case 0x03: return CAT_MODE_CWR;		// FT-1000MP; FT-1000, FT-990: CW narrow
	case 0x04: return CAT_MODE_AM;
	case 0x05: return CAT_MODE_AMS;		// FT-1000MP; FT-1000, FT-990: AM narrow
	case 0x06: case 0x07: return CAT_MODE_FM;
	case 0x08: return CAT_MODE_RTTY;	// RTTY on LSB
	case 0x09: return CAT_MODE_RTTYR;
	case 0x0a: return CAT_MODE_PKTLSB;
	case 0x0b: return CAT_MODE_PKTFM;
	default:   return CAT_MODE_NONE;
	}
}

static enum cat_mode modeset_920(uint8_t c) {
	switch(c & 0x7f) {
	case 0x00: return CAT_MODE_LSB;
	case 0x01: return CAT_MODE_USB;
	case 0x02: return CAT_MODE_CW;		// CW on USB
	case 0x03: return CAT_MODE_CWR;
	case 0x04: case 0x05: return CAT_MODE_AM;
	case 0x06: case 0x07: return CAT_MODE_FM;
	case 0x08: return CAT_MODE_PKTLSB;	// DATA-LSB
	case 0x0a: return CAT_MODE_PKTUSB;
	case 0x0b: return CAT_MODE_PKTFM;
	default:   return CAT_MODE_NONE;
	}
}

// FT-840, FT-890, FT-900 mode set: wide and narrow variants.
static enum cat_mode modeset_900(uint8_t c) {
	switch(c) {
	case 0x00: return CAT_MODE_LSB;
	case 0x01: return CAT_MODE_USB;
	case 0x02: case 0x03: return CAT_MODE_CW;
	case 0x04: case 0x05: return CAT_MODE_AM;
	case 0x06: return CAT_MODE_FM;
	default:   return CAT_MODE_NONE;
	}
}

// FT-757GXII, status data and mode set.
static enum cat_mode mode_757(uint8_t c) {
	switch(c) {
	case 0x00: return CAT_MODE_LSB;
	case 0x01: return CAT_MODE_USB;
	case 0x02: case 0x03: return CAT_MODE_CW;	// wide, narrow
	case 0x04: return CAT_MODE_AM;
	case 0x05: return CAT_MODE_FM;
	default:   return CAT_MODE_NONE;
	}
}

// Mode of a status data record: mode byte m, the byte after it (IF filter) f.
static enum cat_mode rec_mode(const struct cat_ybin *y, uint8_t m, uint8_t f) {
	int alt = f & 0x80;	// CW on LSB, AM sync, RTTY on USB, packet on FM

	switch(y->m->fam) {
	case FAM_920:
		switch(m & 0x47) {	// bit 7: narrow
		case 0x00: return CAT_MODE_LSB;
		case 0x40: return CAT_MODE_USB;
		case 0x01: return CAT_MODE_CWR;
		case 0x41: return CAT_MODE_CW;
		case 0x02: return CAT_MODE_AM;
		case 0x03: return CAT_MODE_FM;
		case 0x04: return CAT_MODE_PKTLSB;
		case 0x05: return CAT_MODE_PKTUSB;
		case 0x06: return CAT_MODE_PKTFM;
		default:   return CAT_MODE_NONE;
		}
	case FAM_900:
		switch(m & 0x07) {
		case 0: return CAT_MODE_LSB;
		case 1: return CAT_MODE_USB;
		case 2: return CAT_MODE_CW;
		case 3: return CAT_MODE_AM;
		case 4: return CAT_MODE_FM;
		default: return CAT_MODE_NONE;
		}
	case FAM_757:
		return mode_757(m);
	default:
		break;
	}
	// FT-1000MP, FT-1000, FT-990
	switch(m & 0x07) {
	case 0: return CAT_MODE_LSB;
	case 1: return CAT_MODE_USB;
	case 2: return alt && y->m->fam == FAM_MP ? CAT_MODE_CWR : CAT_MODE_CW;
	case 3: return alt && y->m->fam == FAM_MP ? CAT_MODE_AMS : CAT_MODE_AM;
	case 4: return CAT_MODE_FM;
	case 5: return alt ? CAT_MODE_RTTYR : CAT_MODE_RTTY;
	case 6: return alt ? CAT_MODE_PKTFM : CAT_MODE_PKTLSB;
	default: return CAT_MODE_NONE;
	}
}

/*
 * State updates. VFOs are addressed as A/B here; cat_state keeps the selected
 * (receive) VFO and the other one. Slot -1: the selected one, whichever it is.
 */

static int slot_of(const struct cat_ybin *y, int ab) {
	if(ab < 0)
		return CAT_VFO_SEL;
	return (uint32_t)ab == y->st->vfo_ab.val ? CAT_VFO_SEL : CAT_VFO_UNSEL;
}

// Every mode code tells whether it is a data mode.
static void set_mode(struct cat_ybin *y, int ab, enum cat_mode mode, uint64_t t) {
	int slot = slot_of(y, ab);
	cat_state_set_mode(y->st, slot, mode, t);
	if(cat_mode_with_data(mode, 1) != mode)
		cat_state_set_data(y->st, slot, 0, t);
}

// A status data record: frequency of n bytes at fo, mode byte at mo, filter byte at mo + fl.
struct rec {
	uint8_t fo, mo, fl;
};

static int rec_apply(struct cat_ybin *y, int ab, const uint8_t *a, struct rec r, uint64_t t) {
	uint32_t hz;
	enum cat_mode mode;

	switch(y->m->fam) {
	case FAM_757:
		if(dec_bcd(a + r.fo, 4, 0, &hz))
			return -1;
		hz *= 10;
		break;
	case FAM_920:
		hz = dec_be(a + r.fo, 4);
		break;
	case FAM_MP:
		hz = (uint32_t)((uint64_t)dec_be(a + r.fo, 4) * 10 / 16);	// 0.625 Hz units
		break;
	default:
		hz = dec_be(a + r.fo, 3) * 10;
		break;
	}
	mode = rec_mode(y, a[r.mo], a[r.mo + r.fl]);
	if(!hz || mode == CAT_MODE_NONE)
		return -1;
	cat_state_set_freq(y->st, slot_of(y, ab), hz, t);
	set_mode(y, ab, mode, t);
	return 1;
}

static void set_split(struct cat_ybin *y, int split, uint64_t t) {
	cat_state_set_split(y->st, split, t);
}

static void select_vfo(struct cat_ybin *y, int ab, uint64_t t) {
	cat_state_select_vfo(y->st, ab, t);
}

// FT-817 family: the EEPROM bytes (read command BB) with VFO and split, as Hamlib reads them.
static int apply_eeprom(struct cat_ybin *y, unsigned addr, uint8_t v, uint64_t t) {
	int ft817 = y->cfg.model == CAT_YBIN_FT817;

	if(y->m->fam != FAM_817)
		return 0;
	if(addr == (ft817 ? 0x55u : 0x68u)) {
		select_vfo(y, v & 0x01 ? VFO_B : VFO_A, t);
		return 1;
	}
	if(addr == (ft817 ? 0x7au : 0x8du)) {
		set_split(y, v & 0x80, t);
		return 1;
	}
	return 0;
}

// Answer a (n bytes, complete) to command c. Returns 1 if the state was updated, 0 if the
// answer carries nothing of interest, -1 if its values don't fit.
static int apply_answer(struct cat_ybin *y, const uint8_t *c, const uint8_t *a, size_t n, uint64_t t) {
	uint8_t op = c[4], u = c[3];
	int r;

	switch(y->m->fam) {
	case FAM_817:
	case FAM_847: {
		uint32_t hz;
		enum cat_mode mode;
		if(op == 0xbb && n == 2)
			return apply_eeprom(y, c[0] << 8 | c[1], a[0], t);
		if(op != 0x03 || n != 5)
			return 0;
		mode = mode_817(a[4]);
		if(dec_bcd(a, 4, 1, &hz) || !hz || mode == CAT_MODE_NONE ||
		   (y->m->fam == FAM_847 && (a[4] & 0x7f) > 0x08))
			return -1;
		cat_state_set_freq(y->st, CAT_VFO_SEL, hz * 10, t);
		set_mode(y, -1, mode, t);
		return 1;
	}
	case FAM_757:
		if((op != 0x10 && op != 0x0e) || n != 75)
			return 0;
		// Flags: bit 3 VFO B, bit 4 memory.
		if(!(a[0] & 0x10))
			select_vfo(y, a[0] & 0x08 ? VFO_B : VFO_A, t);
		if((r = rec_apply(y, -1, a, (struct rec){ 5, 9, 0 }, t)) < 0 ||
		   rec_apply(y, VFO_A, a, (struct rec){ 10, 14, 0 }, t) < 0 ||
		   rec_apply(y, VFO_B, a, (struct rec){ 15, 19, 0 }, t) < 0)
			return -1;
		return r;
	default:
		break;
	}

	if(op == 0xfa) {
		switch(y->m->fam) {
		case FAM_MP:
			// Byte 1: bit 0 split, bit 4 VFO B in use.
			select_vfo(y, a[0] & 0x10 ? VFO_B : VFO_A, t);
			set_split(y, a[0] & 0x01, t);
			return 1;
		case FAM_990:
			select_vfo(y, a[0] & 0x02 ? VFO_B : VFO_A, t);
			set_split(y, a[0] & 0x01, t);
			return 1;
		case FAM_1000:
			// The VFO B bit doesn't work on the FT-1000D (Hamlib).
			set_split(y, a[0] & 0x01, t);
			return 1;
		case FAM_920: {
			// Bit 0: VFO B transmits, bit 1: VFO B receives.
			int rx_b = !!(a[0] & 0x02), tx_b = !!(a[0] & 0x01);
			select_vfo(y, rx_b, t);
			set_split(y, rx_b != tx_b, t);
			return 1;
		}
		case FAM_900:
			// Bit 7: VFO (not memory), bit 6: VFO B, bit 2: split.
			if(a[0] & 0x80)
				select_vfo(y, a[0] & 0x40 ? VFO_B : VFO_A, t);
			set_split(y, a[0] & 0x04, t);
			return 1;
		default:
			return 0;
		}
	}
	if(op != 0x10 || (u != 2 && u != 3))
		return 0;

	switch(y->m->fam) {
	case FAM_900:
		if(u == 2)
			return rec_apply(y, -1, a, (struct rec){ 2, 7, 2 }, t);
		if(rec_apply(y, VFO_A, a, (struct rec){ 1, 6, 2 }, t) < 0)
			return -1;
		return rec_apply(y, VFO_B, a, (struct rec){ 10, 15, 2 }, t);
	case FAM_920:
		// Current data and VFO data alike: VFO A (main display) and VFO B (sub display).
		if(rec_apply(y, VFO_A, a, (struct rec){ 1, 7, 1 }, t) < 0)
			return -1;
		return rec_apply(y, VFO_B, a, (struct rec){ 15, 21, 1 }, t);
	default:
		// FT-1000MP, FT-1000, FT-990: records of 16 bytes. The FT-990 sends two for the current
		// data, the first one is the display.
		if(u == 2)
			return rec_apply(y, -1, a, (struct rec){ 1, 7, 1 }, t);
		if(rec_apply(y, VFO_A, a, (struct rec){ 1, 7, 1 }, t) < 0)
			return -1;
		return rec_apply(y, VFO_B, a + 16, (struct rec){ 1, 7, 1 }, t);
	}
}

// A command from a client: frequency and mode settings are taken over right away (only the
// FT-817 family acknowledges them). Returns 1 if the state was updated, 0 if not, -1 if the
// values don't fit.
static int apply_command(struct cat_ybin *y, const uint8_t *c, uint64_t t) {
	uint8_t op = c[4], u = c[3];
	uint32_t hz;
	enum cat_mode mode;
	int ab;

	switch(y->m->fam) {
	case FAM_817:
	case FAM_847:
		if(op == 0x01) {
			if(dec_bcd(c, 4, 1, &hz) || !hz)
				return -1;
			cat_state_set_freq(y->st, CAT_VFO_SEL, hz * 10, t);
			return 1;
		}
		if(op == 0x07) {
			mode = mode_817(c[0]);
			if(mode == CAT_MODE_NONE)
				return -1;
			set_mode(y, -1, mode, t);
			return 1;
		}
		if(y->m->fam == FAM_847)
			return 0;
		// A/B toggle (Hamlib reads VFO B this way), split on and off.
		if(op == 0x81) {
			select_vfo(y, !y->st->vfo_ab.val, t);
			return 1;
		}
		if(op == 0x02 || op == 0x82) {
			set_split(y, op == 0x02, t);
			return 1;
		}
		return 0;
	default:
		break;
	}

	switch(op) {
	case 0x0a:	// FT-1000MP, FT-920: VFO A; the others: the current VFO
	case 0x8a:	// VFO B
		if(op == 0x8a && y->m->fam != FAM_MP && y->m->fam != FAM_920 && y->m->fam != FAM_1000)
			return 0;
		if(dec_bcd(c, 4, 0, &hz) || !hz)
			return -1;
		ab = op == 0x8a ? VFO_B : (y->m->fam == FAM_MP || y->m->fam == FAM_920) ? VFO_A : -1;
		cat_state_set_freq(y->st, slot_of(y, ab), hz * 10, t);
		return 1;
	case 0x0c:
		switch(y->m->fam) {
		case FAM_757: mode = mode_757(u); ab = -1; break;
		case FAM_900: mode = modeset_900(u); ab = -1; break;
		case FAM_920: mode = modeset_920(u); ab = u & 0x80 ? VFO_B : VFO_A; break;
		case FAM_MP:  mode = modeset_1000(u); ab = u & 0x80 ? VFO_B : VFO_A; break;
		default:      mode = modeset_1000(u); ab = u & 0x80 ? VFO_B : -1; break;
		}
		if(mode == CAT_MODE_NONE)
			return -1;
		set_mode(y, ab, mode, t);
		return 1;
	case 0x05:
		// VFO A/B: the FT-1000 doesn't tell it in its status data.
		if(u > 1)
			return 0;
		select_vfo(y, u, t);
		return 1;
	default:
		return 0;
	}
}

/*
 * Pending commands and their answers
 */

static void pend_remove(struct cat_ybin *y, int i) {
	if(i == 0)
		y->ans_len = 0;
	memmove(&y->pend[i], &y->pend[i + 1], (y->npend - i - 1) * sizeof(y->pend[0]));
	y->npend--;
}

static unsigned byte_gap(const struct cat_ybin *y) {
	return YBIN_BYTE_GAP_MS + y->pacing_ms;
}

// Still waiting for (the rest of) its answer at t.
static int pend_live(const struct cat_ybin *y, const struct ybin_pending *p, uint64_t t) {
	if(p->got)
		return p->t_last + byte_gap(y) >= t;
	return p->t_ms + YBIN_ANSWER_MS + y->pacing_ms >= t;
}

// FT-817 family: the one byte answering a command that is no query.
static int is_ack(const struct cat_ybin *y, const uint8_t *c) {
	if(y->m->fam != FAM_817)
		return 0;
	switch(c[4]) {
	case 0x03: case 0xbb: case 0xbd: case 0xe7: case 0xf7: return 0;
	default: return 1;
	}
}

// The answer of pend[0] is complete, or ended early (complete = 0). Its last piece is
// passed on.
static void answer_done(struct cat_ybin *y, int complete, uint64_t t) {
	struct ybin_pending p = y->pend[0];
	uint8_t a[YBIN_MAX_FRAME];
	size_t n = y->ans_len;
	enum cat_frame_kind kind;
	int r = 0;

	memcpy(a, y->ans, n);
	pend_remove(y, 0);
	// The radio takes the next command now: its answer is due from here on.
	if(y->npend && y->pend[0].t_ms < t)
		y->pend[0].t_ms = t;
	if(!complete) {
		kind = p.var ? CAT_FRAME_REPLY : CAT_FRAME_DROPPED;
		t = p.t_last;
	} else if(p.len > YBIN_MAX_FRAME) {
		kind = CAT_FRAME_REPLY;		// a memory dump, nothing decoded
	} else {
		r = apply_answer(y, p.cmd, a, n, t);
		kind = r < 0 ? CAT_FRAME_BAD : is_ack(y, p.cmd) ? CAT_FRAME_ACK : CAT_FRAME_REPLY;
	}
	if(n)
		emit(y, kind, CAT_DIR_RADIO, a, n, r > 0, p.own, p.client, t);
}

// Answers that ended (no byte for a while) or never came.
static void pend_expire(struct cat_ybin *y, uint64_t t) {
	while(y->npend && !pend_live(y, &y->pend[0], t)) {
		if(y->pend[0].got)
			answer_done(y, 0, t);
		else
			pend_remove(y, 0);
	}
}

// A new command was sent: the oldest ones, without any answer byte for GIVEUP_MS, got none.
static void pend_giveup(struct cat_ybin *y, uint64_t t) {
	while(y->npend && !y->pend[0].got && y->pend[0].t_ms + GIVEUP_MS < t)
		pend_remove(y, 0);
}

static void pend_add(struct cat_ybin *y, const uint8_t *c, unsigned len, int own, int client, uint64_t t) {
	if(y->npend == PEND_MAX) {
		if(y->pend[0].got)
			answer_done(y, 0, t);
		else
			pend_remove(y, 0);
	}
	struct ybin_pending *p = &y->pend[y->npend++];
	memcpy(p->cmd, c, YBIN_CMD_LEN);
	p->len = len & LEN_MASK;
	p->var = !!(len & VAR);
	p->own = own;
	p->client = client;
	p->got = 0;
	p->t_ms = t;
	p->t_last = t;
}

static void command(struct cat_ybin *y, enum cat_dir dir, int client, const uint8_t *c, uint64_t t) {
	int own = dir == CAT_DIR_OWN;
	unsigned len = answer_len(y, c);
	int r;

	if(!y->st->vfo_ab.valid)
		select_vfo(y, VFO_A, t);	// until the radio tells otherwise
	pend_expire(y, t);
	pend_giveup(y, t);
	if(c[4] == 0x0e && has_pacing(y))
		y->pacing_ms = c[3];
	if(len)
		pend_add(y, c, len, own, client, t);
	else
		y->hold[own ? 0 : client] = t + (y->m->hold_ms ? y->m->hold_ms : YBIN_HOLD_MS);
	r = apply_command(y, c, t);
	emit(y, r < 0 ? CAT_FRAME_BAD : CAT_FRAME_CMD, dir, c, YBIN_CMD_LEN, r > 0, own, client, t);
}

static void radio_byte(struct cat_ybin *y, uint8_t b, uint64_t t) {
	pend_expire(y, t);
	if(!y->npend) {
		emit(y, CAT_FRAME_UNMATCHED, CAT_DIR_RADIO, &b, 1, 0, 0, 0, t);
		return;
	}

	struct ybin_pending *p = &y->pend[0];
	if(!y->st->vfo_ab.valid)
		select_vfo(y, VFO_A, t);
	y->ans[y->ans_len++] = b;
	p->got++;
	p->t_last = t;
	if(p->got == p->len) {
		answer_done(y, 1, t);
	} else if(y->ans_len == YBIN_MAX_FRAME) {
		// A long answer: passed on in pieces.
		emit(y, CAT_FRAME_REPLY, CAT_DIR_RADIO, y->ans, y->ans_len, 0, p->own, p->client, t);
		y->ans_len = 0;
	}
}

// The radio forgets a command whose bytes stop: so do we. The frame carries the time of its
// last byte, as all frames do, so CAT monitor traces replay the same way.
static void rx_expire(struct cat_ybin *y, int i, uint64_t t) {
	struct ybin_rx *rx = &y->rx[i];
	if(!rx->len || rx->t_ms + YBIN_CMD_GAP_MS >= t)
		return;
	emit(y, CAT_FRAME_DROPPED, i == RX_OWN ? CAT_DIR_OWN : CAT_DIR_HOST, rx->buf, rx->len, 0, i == RX_OWN,
	     i == RX_OWN ? 0 : i + 1, rx->t_ms);
	rx->len = 0;
}

static void host_byte(struct cat_ybin *y, enum cat_dir dir, int client, struct ybin_rx *rx, uint8_t b,
		      uint64_t t) {
	rx_expire(y, rx - y->rx, t);
	rx->t_ms = t;
	rx->buf[rx->len++] = b;
	if(rx->len == YBIN_CMD_LEN) {
		rx->len = 0;
		command(y, dir, client, rx->buf, t);
	}
}

static int rx_partial(const struct ybin_rx *rx, uint64_t t_ms) {
	return rx->len && rx->t_ms + YBIN_CMD_GAP_MS >= t_ms;
}

unsigned cat_ybin_pending(const struct cat_ybin *y, uint64_t t_ms) {
	unsigned r = 0;

	for(int c = 0; c < CAT_MAX_CLIENTS; c++)
		if(rx_partial(&y->rx[c], t_ms) || y->hold[c + 1] > t_ms)
			r |= CAT_PENDING_CLIENT;
	if(rx_partial(&y->rx[RX_OWN], t_ms) || y->hold[0] > t_ms)
		r |= CAT_PENDING_OWN;
	for(int i = 0; i < y->npend; i++)
		if(pend_live(y, &y->pend[i], t_ms))
			r |= y->pend[i].own ? CAT_PENDING_OWN : CAT_PENDING_CLIENT;
	return r;
}

int cat_ybin_client_busy(const struct cat_ybin *y, int client, uint64_t t_ms) {
	if(client < 1 || client > CAT_MAX_CLIENTS)
		return 0;
	if(rx_partial(&y->rx[client - 1], t_ms) || y->hold[client] > t_ms)
		return 1;
	for(int i = 0; i < y->npend; i++)
		if(y->pend[i].client == client && !y->pend[i].own && pend_live(y, &y->pend[i], t_ms))
			return 1;
	return 0;
}

void cat_ybin_client_discard(struct cat_ybin *y, int client) {
	if(client < 1 || client > CAT_MAX_CLIENTS)
		return;
	y->rx[client - 1].len = 0;
	y->hold[client] = 0;
	for(int i = y->npend - 1; i >= 0; i--) {
		if(y->pend[i].client != client || y->pend[i].own)
			continue;
		if(i == 0 && y->pend[0].got)
			answer_done(y, 0, y->pend[0].t_last);
		else
			pend_remove(y, i);
	}
}

void cat_ybin_feed(struct cat_ybin *y, enum cat_dir dir, int client, const uint8_t *buf, size_t len,
		   uint64_t t_ms) {
	struct ybin_rx *rx;

	switch(dir) {
	case CAT_DIR_HOST:
		if(client < 1 || client > CAT_MAX_CLIENTS)
			return;
		rx = &y->rx[client - 1];
		break;
	case CAT_DIR_OWN:
		rx = &y->rx[RX_OWN];
		client = 0;
		break;
	case CAT_DIR_RADIO:
		for(size_t i = 0; i < len; i++)
			radio_byte(y, buf[i], t_ms);
		return;
	default:
		return;
	}
	for(size_t i = 0; i < len; i++)
		host_byte(y, dir, client, rx, buf[i], t_ms);
}

int cat_ybin_answering(const struct cat_ybin *y, uint64_t t_ms, int *client, int *own) {
	if(!y->npend || !y->pend[0].got || !pend_live(y, &y->pend[0], t_ms))
		return 0;
	*client = y->pend[0].client;
	*own = y->pend[0].own;
	return 1;
}

void cat_ybin_expire(struct cat_ybin *y, uint64_t t_ms) {
	for(int i = 0; i < RX_NUM; i++)
		rx_expire(y, i, t_ms);
	pend_expire(y, t_ms);
}

size_t cat_ybin_query(const struct cat_ybin *y, uint8_t *buf) {
	memset(buf, 0, YBIN_CMD_LEN);
	switch(y->m->fam) {
	case FAM_757:
		buf[4] = 0x10;		// all status data, 75 bytes
		break;
	case FAM_817:
	case FAM_847:
		buf[4] = 0x03;		// frequency and mode
		break;
	default:
		buf[3] = 0x02;		// current operating data
		buf[4] = 0x10;
		break;
	}
	return answer_len(y, buf) & LEN_MASK;
}

size_t cat_ybin_cat_on(const struct cat_ybin *y, uint8_t *buf) {
	if(y->m->fam != FAM_847)
		return 0;
	memset(buf, 0, YBIN_CMD_LEN);	// CAT ON: opcode 00
	return YBIN_CMD_LEN;
}
