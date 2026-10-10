/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdlib.h>
#include <string.h>
#include "ybin_radio.h"

#define CMD_GAP 0.2	// the radio forgets a command whose bytes stop this long

enum fam { F757, F817, F847, F900, F920, F990, F1000, FMP };

static const struct {
	const char *name;
	int fam;
	int dump;	// all status data (0x10, U = 0), bytes
} models[] = {
	{ "ft757gx2", F757, 0 },
	{ "ft817", F817, 0 },
	{ "ft857", F817, 0 },
	{ "ft897", F817, 0 },
	{ "ft847", F847, 0 },
	{ "ft840", F900, 649 },
	{ "ft890", F900, 649 },
	{ "ft900", F900, 1941 },
	{ "ft920", F920, 0 },
	{ "ft990", F990, 1508 },
	{ "ft1000", F1000, 1636 },
	{ "ft1000mp", FMP, 1863 },
};

struct ybin_radio {
	int model;
	int fam;
	int ack;
	struct ybr_state st;
	struct ybr_stats stats;
	uint8_t cmd[5];
	int len;
	double t_last;
};

struct ybin_radio *ybr_create(const char *model) {
	for(size_t i = 0; i < sizeof(models) / sizeof(models[0]); i++) {
		if(strcmp(model, models[i].name))
			continue;
		struct ybin_radio *r = calloc(1, sizeof(*r));
		if(!r)
			return NULL;
		r->model = i;
		r->fam = models[i].fam;
		r->ack = 1;
		r->st.freq[0] = 14074000;
		r->st.freq[1] = 14250000;
		r->st.mode[0] = YBR_USB;
		r->st.mode[1] = YBR_USB;
		return r;
	}
	return NULL;
}

void ybr_destroy(struct ybin_radio *r) {
	free(r);
}

struct ybr_state *ybr_state(struct ybin_radio *r) {
	return &r->st;
}

const struct ybr_stats *ybr_stats(const struct ybin_radio *r) {
	return &r->stats;
}

void ybr_set_ack(struct ybin_radio *r, int on) {
	r->ack = on;
}

/*
 * Encoding
 */

static void put_be(uint8_t *p, uint32_t v, int n) {
	for(int i = n - 1; i >= 0; i--, v >>= 8)
		p[i] = v & 0xff;
}

// v as 8 BCD digits in 4 bytes, most significant byte first (be) or last.
static void put_bcd(uint8_t *p, uint32_t v, int be) {
	for(int i = 0; i < 4; i++, v /= 100) {
		uint8_t b = (v % 100 / 10) << 4 | v % 10;
		p[be ? 3 - i : i] = b;
	}
}

static uint32_t get_bcd(const uint8_t *p, int be) {
	uint32_t v = 0;
	for(int i = 0; i < 4; i++) {
		uint8_t b = p[be ? i : 3 - i];
		v = v * 100 + (b >> 4) * 10 + (b & 0x0f);
	}
	return v;
}

// FT-817 family and FT-847 mode codes.
static uint8_t code_817(int m) {
	switch(m) {
	case YBR_LSB: return 0x00;
	case YBR_USB: return 0x01;
	case YBR_CW:  return 0x02;
	case YBR_CWR: return 0x03;
	case YBR_AM: case YBR_AMS: return 0x04;
	case YBR_WFM: return 0x06;
	case YBR_PKTFM: return 0x0c;
	case YBR_FM: return 0x08;
	default: return 0x0a;	// DIG
	}
}

static int mode_817(uint8_t c) {
	switch(c & 0x7f) {
	case 0x00: return YBR_LSB;
	case 0x01: return YBR_USB;
	case 0x02: return YBR_CW;
	case 0x03: return YBR_CWR;
	case 0x04: return YBR_AM;
	case 0x06: return YBR_WFM;
	case 0x08: return YBR_FM;
	case 0x0a: return YBR_DIG;
	case 0x0c: return YBR_PKTFM;
	default: return -1;
	}
}

// Mode set (0x0c) parameter, without the VFO B bit.
static int modeset(int fam, uint8_t c) {
	switch(fam) {
	case F757: {
		static const int m[] = { YBR_LSB, YBR_USB, YBR_CW, YBR_CW, YBR_AM, YBR_FM };
		return c < 6 ? m[c] : -1;
	}
	case F900: {
		static const int m[] = { YBR_LSB, YBR_USB, YBR_CW, YBR_CW, YBR_AM, YBR_AM, YBR_FM };
		return c < 7 ? m[c] : -1;
	}
	case F920: {
		static const int m[] = { YBR_LSB, YBR_USB, YBR_CW, YBR_CWR, YBR_AM, YBR_AM, YBR_FM, YBR_FM,
					 YBR_PKTL, -1, YBR_PKTU, YBR_PKTFM };
		return c <= 0x0b ? m[c] : -1;
	}
	default: {
		static const int m[] = { YBR_LSB, YBR_USB, YBR_CW, YBR_CWR, YBR_AM, YBR_AMS, YBR_FM, YBR_FM,
					 YBR_RTTY, YBR_RTTYR, YBR_PKTL, YBR_PKTFM };
		int r = c <= 0x0b ? m[c] : -1;
		// FT-1000, FT-990: 03 and 05 are CW and AM with the narrow filter
		if(fam != FMP && (r == YBR_CWR || r == YBR_AMS))
			r = r == YBR_CWR ? YBR_CW : YBR_AM;
		return r;
	}
	}
}

// 16 byte record of the FT-1000MP, FT-1000, FT-990.
static void rec16(const struct ybin_radio *r, uint8_t *p, uint32_t hz, int m) {
	static const uint8_t code[] = {
		[YBR_LSB] = 0, [YBR_USB] = 1, [YBR_CW] = 2, [YBR_CWR] = 2, [YBR_AM] = 3, [YBR_AMS] = 3,
		[YBR_FM] = 4, [YBR_WFM] = 4, [YBR_RTTY] = 5, [YBR_RTTYR] = 5, [YBR_PKTL] = 6, [YBR_PKTU] = 6,
		[YBR_PKTFM] = 6, [YBR_DIG] = 6,
	};
	int alt = m == YBR_RTTYR || m == YBR_PKTFM || (r->fam == FMP && (m == YBR_CWR || m == YBR_AMS));

	memset(p, 0, 16);
	p[0] = 0x11;	// band 14.0-14.5 MHz, whatever the frequency
	if(r->fam == FMP)
		put_be(p + 1, (uint32_t)((uint64_t)hz * 16 / 10), 4);
	else
		put_be(p + 1, hz / 10, 3);
	p[7] = code[m];
	p[8] = alt ? 0x80 : 0x00;
}

static void rec14(uint8_t *p, uint32_t hz, int m) {
	static const uint8_t code[] = {
		[YBR_LSB] = 0x00, [YBR_USB] = 0x40, [YBR_CW] = 0x41, [YBR_CWR] = 0x01, [YBR_AM] = 0x02,
		[YBR_AMS] = 0x02, [YBR_FM] = 0x03, [YBR_WFM] = 0x03, [YBR_RTTY] = 0x04, [YBR_RTTYR] = 0x05,
		[YBR_PKTL] = 0x04, [YBR_PKTU] = 0x05, [YBR_PKTFM] = 0x06, [YBR_DIG] = 0x05,
	};
	memset(p, 0, 14);
	put_be(p + 1, hz, 4);
	p[7] = code[m];
}

static uint8_t code_900(int m) {
	switch(m) {
	case YBR_LSB: return 0;
	case YBR_CW: case YBR_CWR: return 2;
	case YBR_AM: case YBR_AMS: return 3;
	case YBR_FM: case YBR_WFM: return 4;
	default: return 1;
	}
}

static uint8_t code_757(int m) {
	switch(m) {
	case YBR_LSB: return 0;
	case YBR_CW: case YBR_CWR: return 2;
	case YBR_AM: case YBR_AMS: return 4;
	case YBR_FM: case YBR_WFM: return 5;
	default: return 1;
	}
}

/*
 * Commands
 */

static size_t cmd_817(struct ybin_radio *r, const uint8_t *c, uint8_t *out) {
	struct ybr_state *s = &r->st;
	int v = s->vfo, m;

	switch(c[4]) {
	case 0x01:
		s->freq[v] = get_bcd(c, 1) * 10;
		break;
	case 0x07:
		if((m = mode_817(c[0])) >= 0)
			s->mode[v] = m;
		break;
	case 0x03:
		put_bcd(out, s->freq[v] / 10, 1);
		out[4] = code_817(s->mode[v]);
		return 5;
	case 0x81:
		s->vfo = !s->vfo;
		break;
	case 0x02: case 0x82:
		s->split = c[4] == 0x02;
		break;
	case 0xe7:
		out[0] = 0x09;		// S9, squelch open
		return 1;
	case 0xf7:
		out[0] = 0xff;		// receiving
		return 1;
	case 0xbb: {
		// EEPROM: 0x55 bit 0 VFO B, 0x7a bit 7 split, 0x65 DIG mode USER-U.
		int addr = c[0] << 8 | c[1];
		out[0] = addr == 0x55 ? s->vfo : addr == 0x7a ? (s->split ? 0x80 : 0) : addr == 0x65 ? 0x80 : 0;
		out[1] = 0;
		return 2;
	}
	case 0xbd:
		out[0] = out[1] = 0;
		return 2;
	case 0xff:
		return 0;		// wake-up
	default:
		break;
	}
	if(!r->ack)
		return 0;
	out[0] = 0x00;
	return 1;
}

static size_t cmd_847(struct ybin_radio *r, const uint8_t *c, uint8_t *out) {
	struct ybr_state *s = &r->st;
	int m;

	if(c[4] == 0x00 || c[4] == 0x80) {
		s->cat_on = c[4] == 0x00;
		return 0;
	}
	if(!s->cat_on)
		return 0;
	switch(c[4]) {
	case 0x01:
		s->freq[0] = get_bcd(c, 1) * 10;
		return 0;
	case 0x07:
		if((m = mode_817(c[0])) >= 0 && m != YBR_DIG && m != YBR_PKTFM && m != YBR_WFM)
			s->mode[0] = m;
		return 0;
	case 0x03: case 0x13: case 0x23:
		put_bcd(out, s->freq[0] / 10, 1);
		out[4] = code_817(s->mode[0]);
		return 5;
	case 0xe7: case 0xf7:
		out[0] = 0x00;
		return 1;
	default:
		r->stats.unknown++;
		return 0;
	}
}

// Status update (0x10) of the models with data records.
static size_t status(struct ybin_radio *r, const uint8_t *c, uint8_t *out) {
	struct ybr_state *s = &r->st;
	int v = s->vfo, u = c[3];

	if(r->fam == F757) {
		if(u == 1) {
			out[0] = 0x9a;		// meter
			return 1;
		}
		memset(out, 0, 75);
		out[0] = s->vfo ? 0x08 : 0x00;
		put_bcd(out + 5, s->freq[v] / 10, 0);
		out[9] = code_757(s->mode[v]);
		put_bcd(out + 10, s->freq[0] / 10, 0);
		out[14] = code_757(s->mode[0]);
		put_bcd(out + 15, s->freq[1] / 10, 0);
		out[19] = code_757(s->mode[1]);
		return 75;
	}
	if(u == 1) {
		out[0] = 0x05;		// memory channel
		return 1;
	}
	if(u == 0) {
		int n = models[r->model].dump;
		for(int i = 0; i < n; i++)
			out[i] = i & 0xff;
		return n;
	}

	switch(r->fam) {
	case F900:
		if(u == 2 || u == 4) {
			memset(out, 0, 19);
			put_be(out + 2, s->freq[v] / 10, 3);
			out[7] = code_900(s->mode[v]);
			return 19;
		}
		if(u == 3) {
			memset(out, 0, 18);
			put_be(out + 1, s->freq[0] / 10, 3);
			out[6] = code_900(s->mode[0]);
			put_be(out + 10, s->freq[1] / 10, 3);
			out[15] = code_900(s->mode[1]);
			return 18;
		}
		return 0;
	case F920:
		if(u == 2 || u == 3) {
			rec14(out, s->freq[0], s->mode[0]);
			rec14(out + 14, s->freq[1], s->mode[1]);
			return 28;
		}
		if(u == 4) {
			rec14(out, s->freq[0], s->mode[0]);
			return 14;
		}
		return 0;
	default:
		if(u == 2) {
			rec16(r, out, s->freq[v], s->mode[v]);
			if(r->fam != F990)
				return 16;
			rec16(r, out + 16, s->freq[v], s->mode[v]);	// the rear display, here the same
			return 32;
		}
		if(u == 3) {
			rec16(r, out, s->freq[0], s->mode[0]);
			rec16(r, out + 16, s->freq[1], s->mode[1]);
			return 32;
		}
		if(u == 4) {
			rec16(r, out, s->freq[0], s->mode[0]);
			return 16;
		}
		return 0;
	}
}

static size_t flags(struct ybin_radio *r, const uint8_t *c, uint8_t *out) {
	struct ybr_state *s = &r->st;

	switch(r->fam) {
	case FMP:
		memset(out, 0, 6);
		out[0] = (s->split ? 0x01 : 0) | (s->vfo ? 0x10 : 0);
		if(c[3] & 1)
			return 6;
		out[3] = 0x03;		// model ID
		out[4] = 0x93;
		return 5;
	case F920:
		memset(out, 0, 8);
		// bit 1: VFO B receives, bit 0: VFO B transmits
		out[0] = (s->vfo ? 0x02 : 0) | ((s->vfo ^ s->split) ? 0x01 : 0);
		return 8;
	case F900:
		memset(out, 0, 5);
		out[0] = 0x80 | (s->vfo ? 0x40 : 0) | (s->split ? 0x04 : 0);
		return 5;
	case F990:
		memset(out, 0, 5);
		out[0] = (s->split ? 0x01 : 0) | (s->vfo ? 0x02 : 0);
		return 5;
	case F1000:
		memset(out, 0, 5);
		out[0] = s->split ? 0x01 : 0;	// the VFO B bit doesn't work
		return 5;
	default:
		return 0;
	}
}

static size_t cmd_rec(struct ybin_radio *r, const uint8_t *c, uint8_t *out) {
	struct ybr_state *s = &r->st;
	int v, m;

	switch(c[4]) {
	case 0x10:
		return status(r, c, out);
	case 0xfa:
		return flags(r, c, out);
	case 0xf7:
		if(r->fam == F920 || r->fam == F757)
			break;
		memset(out, 0x80, 4);
		out[4] = 0xf7;
		return 5;
	case 0x0e:
		s->pacing_ms = c[3];
		if(r->fam == F757) {
			uint8_t q[5] = { 0, 0, 0, 0, 0x10 };
			return status(r, q, out);
		}
		return 0;
	case 0x0a:
	case 0x8a:
		if(c[4] == 0x8a && r->fam != FMP && r->fam != F920 && r->fam != F1000)
			break;
		v = c[4] == 0x8a ? 1 : (r->fam == FMP || r->fam == F920) ? 0 : s->vfo;
		s->freq[v] = get_bcd(c, 0) * 10;
		return 0;
	case 0x0c:
		m = modeset(r->fam, c[3] & 0x7f);
		if(m < 0)
			break;
		v = r->fam == F757 || r->fam == F900 || r->fam == F990 ? s->vfo :
		    c[3] & 0x80 ? 1 : r->fam == F1000 ? s->vfo : 0;
		s->mode[v] = m;
		return 0;
	case 0x05:
		s->vfo = c[3] & 1;
		return 0;
	case 0x01:
		s->split = c[3] & 1;
		return 0;
	default:
		break;
	}
	r->stats.unknown++;
	return 0;
}

size_t ybr_rx(struct ybin_radio *r, uint8_t b, double t, uint8_t *out) {
	size_t n;

	if(r->len && t - r->t_last > CMD_GAP) {
		r->stats.partial++;
		r->len = 0;
	}
	r->t_last = t;
	r->cmd[r->len++] = b;
	if(r->len < 5)
		return 0;
	r->len = 0;
	r->stats.commands++;

	switch(r->fam) {
	case F817: n = cmd_817(r, r->cmd, out); break;
	case F847: n = cmd_847(r, r->cmd, out); break;
	default:   n = cmd_rec(r, r->cmd, out); break;
	}
	if(n)
		r->stats.answered++;
	return n;
}
