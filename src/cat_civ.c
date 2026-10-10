/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdlib.h>
#include <string.h>
#include "cat_civ.h"

#define CIV_PRE   0xfe
#define CIV_EOM   0xfd
#define CIV_COL   0xfc
#define CIV_OK    0xfb
#define CIV_NG    0xfa
#define CIV_BCAST 0x00

#define CIV_HDR_LEN 5			// FE FE to from cmd
#define CIV_MIN_LEN (CIV_HDR_LEN + 1)	// + FD

#define PEND_MAX 8
#define PEND_TIMEOUT_MS 2000

enum {
	RX_IDLE,
	RX_PRE,		// got one FE
	RX_IN,		// inside a frame
	RX_SKIP,	// overlong frame, wait for FD
};

struct civ_rx {
	uint8_t buf[CIV_MAX_FRAME];
	size_t len;
	int state;
	uint64_t t_ms;		// last byte
};

// A framer per source: one per client (index client - 1), mhuxd's own queries, the radio.
#define RX_OWN   CAT_MAX_CLIENTS
#define RX_RADIO (CAT_MAX_CLIENTS + 1)
#define RX_NUM   (CAT_MAX_CLIENTS + 2)

// A command seen as host data or as bus echo, waiting for its reply or FB/FA.
struct civ_pending {
	uint8_t buf[CIV_MAX_FRAME];
	size_t len;
	uint64_t t_ms;
	uint8_t from_host;	// seen as host/own data
	uint8_t echoed;		// seen on the radio stream
	uint8_t own;		// mhuxd's own query
	uint8_t client;		// the client that sent it, 0 if only seen on the bus
	uint8_t lost;		// destroyed in a bus collision, not sent again yet
	uint8_t resends;
};

struct cat_civ {
	struct cat_civ_cfg cfg;
	struct cat_state *st;
	cat_frame_cb cb;
	void *cb_data;
	struct civ_rx rx[RX_NUM];
	struct civ_pending pend[PEND_MAX];
	int npend;
	// previous frame, to learn the radio address from a command/reply pair
	uint8_t last[CIV_MAX_FRAME];
	size_t last_len;
	enum cat_dir last_dir;
	int last_client;
	struct cat_stats stats;
};

#define F_TO(f)   ((f)[2])
#define F_FROM(f) ((f)[3])
#define F_CMD(f)  ((f)[4])
#define F_DATA(f) ((f) + CIV_HDR_LEN)
#define F_DLEN(n) ((n) - CIV_MIN_LEN)

struct cat_civ *cat_civ_create(const struct cat_civ_cfg *cfg, struct cat_state *st) {
	struct cat_civ *civ = calloc(1, sizeof(*civ));
	if(!civ)
		return NULL;
	if(cfg)
		civ->cfg = *cfg;
	if(!civ->cfg.freq_len)
		civ->cfg.freq_len = 5;
	civ->st = st;
	return civ;
}

void cat_civ_destroy(struct cat_civ *civ) {
	free(civ);
}

void cat_civ_set_frame_cb(struct cat_civ *civ, cat_frame_cb cb, void *user_data) {
	civ->cb = cb;
	civ->cb_data = user_data;
}

uint8_t cat_civ_radio_addr(const struct cat_civ *civ) {
	return civ->cfg.radio_addr;
}

const struct cat_stats *cat_civ_get_stats(const struct cat_civ *civ) {
	return &civ->stats;
}

static void emit(struct cat_civ *civ, enum cat_frame_kind kind, enum cat_dir dir,
		 const uint8_t *buf, size_t len, int applied, int own, int client, uint64_t t_ms) {
	switch(kind) {
	case CAT_FRAME_REPLY:     civ->stats.replies++; break;
	case CAT_FRAME_UNMATCHED: civ->stats.unmatched++; break;
	case CAT_FRAME_BAD:       civ->stats.bad++; break;
	case CAT_FRAME_JAM:       civ->stats.jams++; break;
	case CAT_FRAME_DROPPED:   civ->stats.dropped++; break;
	case CAT_FRAME_FOREIGN:   civ->stats.foreign++; break;
	default: break;
	}
	if(applied)
		civ->stats.applied++;
	if(own && dir == CAT_DIR_RADIO)
		civ->stats.own++;

	if(civ->cb) {
		struct cat_frame f = {
			.kind = kind, .dir = dir, .t_ms = t_ms,
			.buf = buf, .len = len, .applied = applied, .own = own, .client = client,
		};
		civ->cb(&f, civ->cb_data);
	}
}

/*
 * Field decoding. Each returns 0 if the value is valid.
 */

static int dec_freq(const struct cat_civ *civ, const uint8_t *p, size_t n, uint32_t *hz) {
	uint64_t f = 0, mul = 1;

	if(n != civ->cfg.freq_len)
		return -1;
	for(size_t i = 0; i < n; i++) {
		uint8_t lo = p[i] & 0x0f, hi = p[i] >> 4;
		if(lo > 9 || hi > 9)
			return -1;
		f += (lo + hi * 10) * mul;
		mul *= 100;
	}
	if(f == 0 || f > UINT32_MAX)
		return -1;
	*hz = (uint32_t)f;
	return 0;
}

static int dec_mode(uint8_t b, enum cat_mode *mode) {
	switch(b) {
	case 0x00: *mode = CAT_MODE_LSB; break;
	case 0x01: *mode = CAT_MODE_USB; break;
	case 0x02: *mode = CAT_MODE_AM; break;
	case 0x03: *mode = CAT_MODE_CW; break;
	case 0x04: *mode = CAT_MODE_RTTY; break;
	case 0x05: *mode = CAT_MODE_FM; break;
	case 0x06: *mode = CAT_MODE_WFM; break;
	case 0x07: *mode = CAT_MODE_CWR; break;
	case 0x08: *mode = CAT_MODE_RTTYR; break;
	case 0x11: case 0x14: case 0x15:	// S-AM (D/L/U)
		*mode = CAT_MODE_AMS; break;
	case 0x12: *mode = CAT_MODE_PSK; break;
	case 0x13: *mode = CAT_MODE_PSKR; break;
	case 0x16: case 0x17: case 0x18: case 0x19:	// P25, DV, dPMR, NXDN-VN
	case 0x20: case 0x21: case 0x22: case 0x23:	// NXDN-N, DCR, DD, ATV
		*mode = CAT_MODE_OTHER; break;
	default:
		return -1;
	}
	return 0;
}

// Data mode byte of 1A 06 and 26: 00 = off, 01..03 = DATA1..3.
static int dec_data(uint8_t b, int *data) {
	if(b > 3)
		return -1;
	*data = b != 0;
	return 0;
}

/*
 * Decoders for frames that carry state. 'vfo' selects the slot for commands
 * without a VFO byte. Return 1 if the state was updated, 0 if the frame
 * carries nothing of interest, -1 if its layout or values are invalid.
 */

static int apply_freq(struct cat_civ *civ, int vfo, const uint8_t *p, size_t n, uint64_t t) {
	uint32_t hz;
	if(dec_freq(civ, p, n, &hz))
		return -1;
	cat_state_set_freq(civ->st, vfo, hz, t);
	return 1;
}

// mode [filter]
static int apply_mode(struct cat_civ *civ, int vfo, const uint8_t *p, size_t n, uint64_t t) {
	enum cat_mode mode;
	if(n < 1 || n > 2 || dec_mode(p[0], &mode))
		return -1;
	cat_state_set_mode(civ->st, vfo, mode, t);
	return 1;
}

// 25: vfo freq
static int apply_25(struct cat_civ *civ, const uint8_t *p, size_t n, uint64_t t) {
	if(n < 1 || p[0] > 1)
		return -1;
	return apply_freq(civ, p[0], p + 1, n - 1, t);
}

// 26: vfo mode [data [filter]]. A set without data byte means DATA OFF.
static int apply_26(struct cat_civ *civ, const uint8_t *p, size_t n, uint64_t t) {
	enum cat_mode mode;
	int data = 0;
	if(n < 2 || n > 4 || p[0] > 1 || dec_mode(p[1], &mode))
		return -1;
	if(n >= 3 && dec_data(p[2], &data))
		return -1;
	cat_state_set_mode(civ->st, p[0], mode, t);
	cat_state_set_data(civ->st, p[0], data, t);
	return 1;
}

// 1A 06: data [filter]
static int apply_1a06(struct cat_civ *civ, const uint8_t *p, size_t n, uint64_t t) {
	int data;
	if(n < 2 || n > 3 || dec_data(p[1], &data))
		return -1;
	cat_state_set_data(civ->st, CAT_VFO_SEL, data, t);
	return 1;
}

// 0F: 00 = split off, 01 = on, 10..12 = simplex/duplex (not tracked)
static int apply_0f(struct cat_civ *civ, const uint8_t *p, size_t n, uint64_t t) {
	if(n != 1)
		return -1;
	if(p[0] <= 1) {
		cat_state_set_split(civ->st, p[0], t);
		return 1;
	}
	return (p[0] >= 0x10 && p[0] <= 0x12) ? 0 : -1;
}

// 07: VFO selection
static int apply_07(struct cat_civ *civ, const uint8_t *p, size_t n, uint64_t t) {
	if(n != 1)
		return -1;
	switch(p[0]) {
	case 0x00: cat_state_select_vfo(civ->st, 0, t); return 1;
	case 0x01: cat_state_select_vfo(civ->st, 1, t); return 1;
	case 0xa0: cat_state_equalize_vfos(civ->st, t); return 1;
	case 0xb0: cat_state_exchange_vfos(civ->st, t); return 1;
	default:   return 0;	// main/sub band, dual watch: not tracked
	}
}

// Reply to a query, frame from the radio.
static int apply_reply(struct cat_civ *civ, const uint8_t *f, size_t len, uint64_t t) {
	const uint8_t *p = F_DATA(f);
	size_t n = F_DLEN(len);

	switch(F_CMD(f)) {
	case 0x03: return apply_freq(civ, CAT_VFO_SEL, p, n, t);
	case 0x04: return apply_mode(civ, CAT_VFO_SEL, p, n, t);
	case 0x0f: return apply_0f(civ, p, n, t);
	case 0x25: return apply_25(civ, p, n, t);
	case 0x26: return apply_26(civ, p, n, t);
	case 0x1a: return (n >= 1 && p[0] == 0x06) ? apply_1a06(civ, p, n, t) : 0;
	default:   return 0;
	}
}

// Transceive broadcast, frame from the radio to 00.
static int apply_unsolicited(struct cat_civ *civ, const uint8_t *f, size_t len, uint64_t t) {
	const uint8_t *p = F_DATA(f);
	size_t n = F_DLEN(len);

	switch(F_CMD(f)) {
	case 0x00: return apply_freq(civ, CAT_VFO_SEL, p, n, t);
	case 0x01: return apply_mode(civ, CAT_VFO_SEL, p, n, t);
	default:   return 0;
	}
}

// Command the radio acknowledged with FB.
static int apply_set(struct cat_civ *civ, const uint8_t *f, size_t len, uint64_t t) {
	const uint8_t *p = F_DATA(f);
	size_t n = F_DLEN(len);

	switch(F_CMD(f)) {
	case 0x05: return apply_freq(civ, CAT_VFO_SEL, p, n, t);
	case 0x06: return apply_mode(civ, CAT_VFO_SEL, p, n, t);
	case 0x07: return apply_07(civ, p, n, t);
	case 0x0f: return apply_0f(civ, p, n, t);
	case 0x25: return apply_25(civ, p, n, t);
	case 0x26: return apply_26(civ, p, n, t);
	case 0x1a: return (n >= 1 && p[0] == 0x06) ? apply_1a06(civ, p, n, t) : 0;
	default:   return 0;
	}
}

// Commands whose first data byte is a sub command or VFO selector, repeated in the reply.
static int has_sub(uint8_t cmd) {
	switch(cmd) {
	case 0x13: case 0x14: case 0x15: case 0x16: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1e: case 0x21: case 0x25: case 0x26: case 0x27:
		return 1;
	default:
		return 0;
	}
}

// 1 = query, 0 = set, -1 = can't tell (not decoded).
static int is_query(const uint8_t *f, size_t len) {
	size_t n = F_DLEN(len);
	switch(F_CMD(f)) {
	case 0x03: case 0x04: case 0x0f:
		return n == 0;
	case 0x05: case 0x06: case 0x07:
		return 0;
	case 0x25: case 0x26:
		return n == 1;
	case 0x1a:
		return (n >= 1 && F_DATA(f)[0] == 0x06) ? n == 1 : -1;
	default:
		return -1;
	}
}

/*
 * Pending commands
 */

static void pend_remove(struct cat_civ *civ, int i) {
	memmove(&civ->pend[i], &civ->pend[i + 1], (civ->npend - i - 1) * sizeof(civ->pend[0]));
	civ->npend--;
}

static void pend_expire(struct cat_civ *civ, uint64_t t) {
	while(civ->npend && civ->pend[0].t_ms + PEND_TIMEOUT_MS < t)
		pend_remove(civ, 0);
}

static void pend_add(struct cat_civ *civ, const uint8_t *f, size_t len, enum cat_dir dir, int client,
		     uint64_t t) {
	if(civ->npend == PEND_MAX)
		pend_remove(civ, 0);
	struct civ_pending *p = &civ->pend[civ->npend++];
	memcpy(p->buf, f, len);
	p->len = len;
	p->t_ms = t;
	p->from_host = dir != CAT_DIR_RADIO;
	p->echoed = dir == CAT_DIR_RADIO;
	p->own = dir == CAT_DIR_OWN;
	p->client = dir == CAT_DIR_HOST ? client : 0;
	p->lost = 0;
	p->resends = 0;
}

// Remove the pending commands of a controller (and client) up to and including index i:
// the radio answers in order.
static void pend_consume(struct cat_civ *civ, int i) {
	uint8_t ctrl = F_FROM(civ->pend[i].buf), client = civ->pend[i].client;
	for(int j = i; j >= 0; j--)
		if(F_FROM(civ->pend[j].buf) == ctrl && civ->pend[j].client == client)
			pend_remove(civ, j);
}

// Oldest pending query of the controller that the reply answers.
static int pend_find_query(struct cat_civ *civ, const uint8_t *r, size_t rlen) {
	for(int i = 0; i < civ->npend; i++) {
		const uint8_t *q = civ->pend[i].buf;
		size_t qlen = civ->pend[i].len;
		if(F_FROM(q) != F_TO(r) || F_CMD(q) != F_CMD(r))
			continue;
		if(has_sub(F_CMD(r)) && (F_DLEN(qlen) < 1 || F_DLEN(rlen) < 1 ||
					 F_DATA(q)[0] != F_DATA(r)[0]))
			continue;
		if(is_query(q, qlen) == 0)
			continue;
		return i;
	}
	return -1;
}

// Oldest pending command of a controller that FB (ok) or FA (!ok) answers.
// FB never answers a query, FA does (command not supported).
static int pend_find_ack(struct cat_civ *civ, uint8_t ctrl, int ok) {
	for(int i = 0; i < civ->npend; i++) {
		const struct civ_pending *p = &civ->pend[i];
		if(F_FROM(p->buf) == ctrl && !(ok && is_query(p->buf, p->len) == 1))
			return i;
	}
	return -1;
}

// Host data seen before: the radio stream copy is the bus echo.
static int pend_find_echo(struct cat_civ *civ, const uint8_t *f, size_t len) {
	for(int i = 0; i < civ->npend; i++) {
		struct civ_pending *p = &civ->pend[i];
		if(p->from_host && !p->echoed && p->len == len && !memcmp(p->buf, f, len))
			return i;
	}
	return -1;
}

/*
 * Frame classification
 */

static void learn_addr(struct cat_civ *civ, enum cat_dir dir, const uint8_t *f, uint64_t t) {
	const uint8_t *l = civ->last;

	if(dir != CAT_DIR_RADIO)
		return;

	if(F_TO(f) == CIV_BCAST || F_CMD(f) == CIV_OK || F_CMD(f) == CIV_NG) {
		civ->cfg.radio_addr = F_FROM(f);
	} else if(civ->last_len && F_TO(l) == F_FROM(f) && F_FROM(l) == F_TO(f) &&
		  F_CMD(l) == F_CMD(f)) {
		// Command and its reply: the replying side is the radio.
		civ->cfg.radio_addr = F_FROM(f);
		pend_add(civ, l, civ->last_len, civ->last_dir, civ->last_client, t);
	}
}

static void process_radio_frame(struct cat_civ *civ, const uint8_t *f, size_t len, uint64_t t) {
	uint8_t ra = civ->cfg.radio_addr;
	enum cat_dir dir = CAT_DIR_RADIO;
	int i, r, own, client;

	if(F_FROM(f) == ra && F_TO(f) != ra) {
		if(F_TO(f) == CIV_BCAST) {
			r = apply_unsolicited(civ, f, len, t);
			emit(civ, r < 0 ? CAT_FRAME_BAD : CAT_FRAME_UNSOLICITED, dir, f, len, r > 0, 0, 0, t);
			return;
		}

		if(F_CMD(f) == CIV_OK || F_CMD(f) == CIV_NG) {
			int ok = F_CMD(f) == CIV_OK;
			r = 0;
			own = client = 0;
			i = pend_find_ack(civ, F_TO(f), ok);
			if(i >= 0) {
				if(ok && is_query(civ->pend[i].buf, civ->pend[i].len) == 0)
					r = apply_set(civ, civ->pend[i].buf, civ->pend[i].len, t);
				own = civ->pend[i].own;
				client = civ->pend[i].client;
				pend_consume(civ, i);
			}
			emit(civ, ok ? CAT_FRAME_ACK : CAT_FRAME_NAK, dir, f, len, r > 0, own, client, t);
			return;
		}

		i = pend_find_query(civ, f, len);
		if(i < 0) {
			emit(civ, CAT_FRAME_UNMATCHED, dir, f, len, 0, 0, 0, t);
			return;
		}
		own = civ->pend[i].own;
		client = civ->pend[i].client;
		pend_consume(civ, i);
		r = apply_reply(civ, f, len, t);
		emit(civ, r < 0 ? CAT_FRAME_BAD : CAT_FRAME_REPLY, dir, f, len, r > 0, own, client, t);
		return;
	}

	if(F_TO(f) == ra && F_FROM(f) != ra && F_FROM(f) != CIV_BCAST) {
		i = pend_find_echo(civ, f, len);
		if(i >= 0) {
			civ->pend[i].echoed = 1;
			emit(civ, CAT_FRAME_ECHO, dir, f, len, 0, civ->pend[i].own, civ->pend[i].client, t);
		} else {
			// Only seen on the bus: no host tap, or another controller.
			pend_add(civ, f, len, CAT_DIR_RADIO, 0, t);
			emit(civ, CAT_FRAME_CMD, dir, f, len, 0, 0, 0, t);
		}
		return;
	}

	emit(civ, CAT_FRAME_FOREIGN, dir, f, len, 0, 0, 0, t);
}

static void process_frame(struct cat_civ *civ, enum cat_dir dir, int client, const uint8_t *f, size_t len,
			  uint64_t t) {
	int own = dir == CAT_DIR_OWN;

	civ->stats.frames++;
	pend_expire(civ, t);

	if(!civ->cfg.radio_addr)
		learn_addr(civ, dir, f, t);

	if(!civ->cfg.radio_addr) {
		emit(civ, CAT_FRAME_FOREIGN, dir, f, len, 0, own, client, t);
	} else if(dir == CAT_DIR_RADIO) {
		process_radio_frame(civ, f, len, t);
	} else if(F_TO(f) == civ->cfg.radio_addr && F_FROM(f) != CIV_BCAST) {
		pend_add(civ, f, len, dir, client, t);
		emit(civ, CAT_FRAME_CMD, dir, f, len, 0, own, client, t);
	} else {
		emit(civ, CAT_FRAME_FOREIGN, dir, f, len, 0, own, client, t);
	}

	memcpy(civ->last, f, len);
	civ->last_len = len;
	civ->last_dir = dir;
	civ->last_client = client;
}

/*
 * Framer
 */

static void rx_drop(struct cat_civ *civ, enum cat_dir dir, int client, struct civ_rx *rx, uint64_t t) {
	if(rx->len > 2)
		emit(civ, CAT_FRAME_DROPPED, dir, rx->buf, rx->len, 0, dir == CAT_DIR_OWN, client, t);
	rx->len = 0;
}

// A jam on the bus: commands sent shortly before and not echoed were destroyed.
static void mark_lost(struct cat_civ *civ, uint64_t t) {
	for(int i = 0; i < civ->npend; i++) {
		struct civ_pending *p = &civ->pend[i];
		if(p->from_host && !p->echoed && p->t_ms + CIV_COLLISION_MS >= t)
			p->lost = 1;
	}
}

static void rx_jam(struct cat_civ *civ, enum cat_dir dir, int client, struct civ_rx *rx, uint64_t t) {
	static const uint8_t col = CIV_COL;
	if(dir == CAT_DIR_RADIO)
		mark_lost(civ, t);
	rx_drop(civ, dir, client, rx, t);
	emit(civ, CAT_FRAME_JAM, dir, &col, 1, 0, dir == CAT_DIR_OWN, client, t);
	rx->state = RX_IDLE;
}

static void rx_byte(struct cat_civ *civ, enum cat_dir dir, int client, struct civ_rx *rx, uint8_t b,
		    uint64_t t) {
	rx->t_ms = t;

	if(b == CIV_COL) {
		rx_jam(civ, dir, client, rx, t);
		return;
	}

	switch(rx->state) {
	case RX_IDLE:
		if(b == CIV_PRE)
			rx->state = RX_PRE;
		break;
	case RX_PRE:
		if(b == CIV_PRE) {
			rx->buf[0] = rx->buf[1] = CIV_PRE;
			rx->len = 2;
			rx->state = RX_IN;
		} else {
			rx->state = RX_IDLE;
		}
		break;
	case RX_IN:
		if(b == CIV_PRE) {
			if(rx->len > 2) {
				// A new frame starts before this one ended.
				rx_drop(civ, dir, client, rx, t);
				rx->state = RX_PRE;
			}
			// else: extra preamble byte
		} else if(b == CIV_EOM) {
			if(rx->len + 1 >= CIV_MIN_LEN) {
				rx->buf[rx->len++] = b;
				process_frame(civ, dir, client, rx->buf, rx->len, t);
				rx->len = 0;
			} else {
				rx_drop(civ, dir, client, rx, t);
			}
			rx->state = RX_IDLE;
		} else if(rx->len + 1 < CIV_MAX_FRAME) {
			rx->buf[rx->len++] = b;
		} else {
			rx_drop(civ, dir, client, rx, t);
			rx->state = RX_SKIP;
		}
		break;
	case RX_SKIP:
		if(b == CIV_EOM)
			rx->state = RX_IDLE;
		else if(b == CIV_PRE)
			rx->state = RX_PRE;
		break;
	}
}

static int rx_partial(const struct civ_rx *rx, uint64_t t_ms) {
	return rx->state != RX_IDLE && rx->t_ms + PEND_TIMEOUT_MS >= t_ms;
}

unsigned cat_civ_pending(const struct cat_civ *civ, uint64_t t_ms) {
	unsigned r = 0;

	for(int c = 0; c < CAT_MAX_CLIENTS; c++)
		if(rx_partial(&civ->rx[c], t_ms))
			r |= CAT_PENDING_CLIENT;
	for(int i = 0; i < civ->npend; i++)
		if(civ->pend[i].t_ms + PEND_TIMEOUT_MS >= t_ms)
			r |= civ->pend[i].own ? CAT_PENDING_OWN : CAT_PENDING_CLIENT;
	return r;
}

int cat_civ_client_busy(const struct cat_civ *civ, int client, uint64_t t_ms) {
	if(client < 1 || client > CAT_MAX_CLIENTS)
		return 0;
	if(rx_partial(&civ->rx[client - 1], t_ms))
		return 1;
	for(int i = 0; i < civ->npend; i++)
		if(civ->pend[i].client == client && civ->pend[i].t_ms + PEND_TIMEOUT_MS >= t_ms)
			return 1;
	return 0;
}

void cat_civ_client_discard(struct cat_civ *civ, int client) {
	if(client < 1 || client > CAT_MAX_CLIENTS)
		return;
	civ->rx[client - 1].len = 0;
	civ->rx[client - 1].state = RX_IDLE;
	for(int i = civ->npend - 1; i >= 0; i--)
		if(civ->pend[i].client == client)
			pend_remove(civ, i);
}

int cat_civ_lost(const struct cat_civ *civ) {
	int n = 0;
	for(int i = 0; i < civ->npend; i++)
		n += civ->pend[i].lost;
	return n;
}

size_t cat_civ_resend(struct cat_civ *civ, int max, uint64_t t_ms, uint8_t *buf, int *client, int *own) {
	for(int i = 0; i < civ->npend; i++) {
		struct civ_pending *p = &civ->pend[i];
		if(!p->lost)
			continue;
		p->lost = 0;
		if(p->resends >= max)
			continue;
		p->resends++;
		p->t_ms = t_ms;
		memcpy(buf, p->buf, p->len);
		*client = p->client;
		*own = p->own;
		return p->len;
	}
	return 0;
}

void cat_civ_feed(struct cat_civ *civ, enum cat_dir dir, int client, const uint8_t *buf, size_t len,
		  uint64_t t_ms) {
	struct civ_rx *rx;

	switch(dir) {
	case CAT_DIR_HOST:
		if(client < 1 || client > CAT_MAX_CLIENTS)
			return;
		rx = &civ->rx[client - 1];
		break;
	case CAT_DIR_OWN:
		rx = &civ->rx[RX_OWN];
		client = 0;
		break;
	case CAT_DIR_RADIO:
		rx = &civ->rx[RX_RADIO];
		client = 0;
		break;
	default:
		return;
	}
	for(size_t i = 0; i < len; i++)
		rx_byte(civ, dir, client, rx, buf[i], t_ms);
}
