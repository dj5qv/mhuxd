/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "cat_state.h"
#include "mhcontrol.h"

static const struct {
	const char *token;
	int keyer_mode;
} mode_tab[CAT_MODE_MAX] = {
	[CAT_MODE_NONE]   = { "",       -1 },
	[CAT_MODE_LSB]    = { "LSB",    MOD_VOICE },
	[CAT_MODE_USB]    = { "USB",    MOD_VOICE },
	[CAT_MODE_CW]     = { "CW",     MOD_CW },
	[CAT_MODE_CWR]    = { "CWR",    MOD_CW },
	[CAT_MODE_RTTY]   = { "RTTY",   MOD_FSK },
	[CAT_MODE_RTTYR]  = { "RTTYR",  MOD_FSK },
	[CAT_MODE_AM]     = { "AM",     MOD_VOICE },
	[CAT_MODE_AMS]    = { "AMS",    MOD_VOICE },
	[CAT_MODE_FM]     = { "FM",     MOD_VOICE },
	[CAT_MODE_WFM]    = { "WFM",    MOD_VOICE },
	[CAT_MODE_PKTLSB] = { "PKTLSB", MOD_DIGITAL },
	[CAT_MODE_PKTUSB] = { "PKTUSB", MOD_DIGITAL },
	[CAT_MODE_PKTAM]  = { "PKTAM",  MOD_DIGITAL },
	[CAT_MODE_PKTFM]  = { "PKTFM",  MOD_DIGITAL },
	[CAT_MODE_FAX]    = { "FAX",    MOD_DIGITAL },
	[CAT_MODE_PSK]    = { "PSK",    -1 },
	[CAT_MODE_PSKR]   = { "PSKR",   -1 },
	[CAT_MODE_OTHER]  = { "OTHER",  -1 },
};

const char *cat_mode_token(enum cat_mode mode) {
	if((unsigned)mode >= CAT_MODE_MAX)
		return "";
	return mode_tab[mode].token;
}

enum cat_mode cat_mode_from_token(const char *token) {
	if(!token || !*token)
		return CAT_MODE_NONE;
	for(int m = CAT_MODE_NONE + 1; m < CAT_MODE_OTHER; m++)
		if(!strcasecmp(token, mode_tab[m].token))
			return m;
	return CAT_MODE_OTHER;
}

int cat_mode_to_keyer(enum cat_mode mode) {
	if((unsigned)mode >= CAT_MODE_MAX)
		return -1;
	return mode_tab[mode].keyer_mode;
}

enum cat_mode cat_mode_with_data(enum cat_mode base, int data) {
	if(!data)
		return base;
	switch(base) {
	case CAT_MODE_LSB: return CAT_MODE_PKTLSB;
	case CAT_MODE_USB: return CAT_MODE_PKTUSB;
	case CAT_MODE_AM:  return CAT_MODE_PKTAM;
	case CAT_MODE_FM:  return CAT_MODE_PKTFM;
	default:           return base;
	}
}

static enum cat_mode mode_without_data(enum cat_mode mode, int *data) {
	*data = 1;
	switch(mode) {
	case CAT_MODE_PKTLSB: return CAT_MODE_LSB;
	case CAT_MODE_PKTUSB: return CAT_MODE_USB;
	case CAT_MODE_PKTAM:  return CAT_MODE_AM;
	case CAT_MODE_PKTFM:  return CAT_MODE_FM;
	default:
		*data = 0;
		return mode;
	}
}

static void item_set(struct cat_state *st, struct cat_item *it, uint32_t val, uint64_t t_ms) {
	if(!it->valid || it->val != val)
		st->changes++;
	it->val = val;
	it->t_ms = t_ms;
	it->valid = 1;
}

static void item_invalidate(struct cat_state *st, struct cat_item *it) {
	if(it->valid)
		st->changes++;
	it->valid = 0;
}

static int valid_vfo(int vfo) {
	return vfo == CAT_VFO_SEL || vfo == CAT_VFO_UNSEL;
}

void cat_state_init(struct cat_state *st) {
	memset(st, 0, sizeof(*st));
}

void cat_state_set_freq(struct cat_state *st, int vfo, uint32_t hz, uint64_t t_ms) {
	if(valid_vfo(vfo))
		item_set(st, &st->vfo[vfo].freq, hz, t_ms);
}

void cat_state_set_mode(struct cat_state *st, int vfo, enum cat_mode mode, uint64_t t_ms) {
	if(!valid_vfo(vfo) || mode <= CAT_MODE_NONE || mode >= CAT_MODE_MAX)
		return;

	struct cat_vfo_state *v = &st->vfo[vfo];
	int data;
	enum cat_mode base = mode_without_data(mode, &data);

	// The radio may or may not keep the data flag when the base mode changes.
	if(!v->mode.valid || v->mode.val != (uint32_t)base)
		item_invalidate(st, &v->data);
	item_set(st, &v->mode, base, t_ms);
	if(data)
		item_set(st, &v->data, 1, t_ms);
}

void cat_state_set_data(struct cat_state *st, int vfo, int data, uint64_t t_ms) {
	if(valid_vfo(vfo))
		item_set(st, &st->vfo[vfo].data, data ? 1 : 0, t_ms);
}

void cat_state_set_split(struct cat_state *st, int split, uint64_t t_ms) {
	item_set(st, &st->split, split ? 1 : 0, t_ms);
}

static void swap_vfos(struct cat_state *st) {
	struct cat_vfo_state tmp = st->vfo[0];
	if(memcmp(&st->vfo[0], &st->vfo[1], sizeof(tmp)))
		st->changes++;
	st->vfo[0] = st->vfo[1];
	st->vfo[1] = tmp;
}

void cat_state_select_vfo(struct cat_state *st, int ab, uint64_t t_ms) {
	ab = ab ? 1 : 0;
	if(st->vfo_ab.valid) {
		if(st->vfo_ab.val != (uint32_t)ab)
			swap_vfos(st);
	} else {
		// Unknown whether the selection changed: the slot contents are unknown now.
		for(int i = 0; i < 2; i++) {
			item_invalidate(st, &st->vfo[i].freq);
			item_invalidate(st, &st->vfo[i].mode);
			item_invalidate(st, &st->vfo[i].data);
		}
	}
	item_set(st, &st->vfo_ab, ab, t_ms);
}

void cat_state_exchange_vfos(struct cat_state *st, uint64_t t_ms) {
	(void)t_ms;
	swap_vfos(st);
}

void cat_state_equalize_vfos(struct cat_state *st, uint64_t t_ms) {
	(void)t_ms;
	if(memcmp(&st->vfo[0], &st->vfo[1], sizeof(st->vfo[0])))
		st->changes++;
	st->vfo[1] = st->vfo[0];
}

enum cat_mode cat_state_mode(const struct cat_state *st, int vfo) {
	if(!valid_vfo(vfo) || !st->vfo[vfo].mode.valid)
		return CAT_MODE_NONE;
	const struct cat_vfo_state *v = &st->vfo[vfo];
	return cat_mode_with_data(v->mode.val, v->data.valid && v->data.val);
}

uint32_t cat_state_rx_freq(const struct cat_state *st) {
	const struct cat_item *f = &st->vfo[CAT_VFO_SEL].freq;
	return f->valid ? f->val : 0;
}

uint32_t cat_state_tx_freq(const struct cat_state *st) {
	if(st->split.valid && st->split.val) {
		const struct cat_item *f = &st->vfo[CAT_VFO_UNSEL].freq;
		return f->valid ? f->val : 0;
	}
	return cat_state_rx_freq(st);
}

int cat_state_to_radio_info(const struct cat_state *st, uint8_t radio, struct mhc_radio_info *info) {
	const struct cat_item *sel = &st->vfo[CAT_VFO_SEL].freq;
	const struct cat_item *unsel = &st->vfo[CAT_VFO_UNSEL].freq;
	uint32_t sel_hz = sel->valid ? sel->val : 0;
	uint32_t unsel_hz = unsel->valid ? unsel->val : 0;

	enum cat_mode mode = cat_state_mode(st, CAT_VFO_SEL);

	memset(info, 0, sizeof(*info));
	info->radio = radio;
	info->mode = cat_mode_to_keyer(mode);
	snprintf(info->mode_str, sizeof(info->mode_str), "%s", cat_mode_token(mode));
	info->rxFreq = sel_hz;
	info->operFreq = sel_hz;
	info->txFreq = cat_state_tx_freq(st);
	if(st->vfo_ab.valid && st->vfo_ab.val == 1) {
		info->vfoAFreq = unsel_hz;
		info->vfoBFreq = sel_hz;
	} else {
		info->vfoAFreq = sel_hz;
		info->vfoBFreq = unsel_hz;
	}

	return (info->mode >= 0 || sel_hz) ? 0 : -1;
}
