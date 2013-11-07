/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include "mhstates.h"
#include "util.h"
#include "logger.h"

struct citem {
	const char	*key;
	uint8_t		off;
	uint8_t		base_bit;
	uint8_t		width;
};

struct citem mok_items[] = {
{ "txFocus", 0, 0, 1 },
{ "rxFocus", 0, 1, 1 },
{ "stereoFocus", 0, 2, 1 },
{ "dvkFocus", 0, 3, 1 },
{ "earsAuto", 0, 4, 1 },
{ "directControl", 0, 5, 1 },
//{ "reserve1", 0, 6, 1 },
//{ "reserve2", 0, 7, 1 },

{ "lptCw", 0, 8, 1 },
{ "lptCw", 0, 9, 1 },
{ "lptTxFocusPin", 0, 10, 1 },
{ "lptRxFocusPin", 0, 11, 1 },
{ "lptStereoFocusPin", 0, 12, 1 },
{ "focusAuto", 0, 13, 1 },
//{ "reserve3", 0, 14, 1 },
//{ "reserve4", 0, 15, 1 },

{ "ears.left.r1Main", 0, 16, 1 },
{ "ears.left.r1Sub", 0, 17, 1 },
{ "ears.left.scLeft", 0, 18, 1 },
{ "ears.left.scRight", 0, 19, 1 },
{ "ears.left.r2Main", 0, 20, 1 },
{ "ears.left.r2Sub", 0, 21, 1 },
{ "wkTxFocus", 0, 22, 1 },
{ "wkKeyOutFocus", 0, 23, 1 },

{ "ears.right.r1Main", 0, 24, 1 },
{ "ears.right.r1Sub", 0, 25, 1 },
{ "ears.right.scLeft", 0, 26, 1 },
{ "ears.right.scRight", 0, 27, 1 },
{ "ears.right.r2Main", 0, 28, 1 },
{ "ears.right.r2Sub", 0, 29, 1 },
{ "chanFocus", 0, 30, 1 },
{ "lptFsIn", 0, 31, 1 },

{ "a1SteppirVer", 4, 7, 8 },
{ "a2SteppirVer", 5, 7, 8 },
{ "a1SteppirVerHi", 6, 7, 8 },
{ "a2SteppirVerHi", 7, 7, 8 },

};

struct citem mpk_items[] = {
{ "lineUpstream", 0, 0, 1 },
{ "micUpstream", 0, 1, 1 },
{ "downstream", 0, 2, 1 },
{ "audioCToAForced", 0, 3, 1 },
{ "audioAToCForced", 0, 4, 1 },
{ "frontMicSelected", 0, 5, 1 },
//{ "reserve1", 0, 6, 1 },
//{ "reserve2", 0, 7, 1 },

{ "sysPwrVoltage", 1, 7, 8 },
{ "steppirVer", 2, 7, 8 },
{ "steppirVerHi", 3, 7, 8 },
};

static const struct citem *find_citem(struct citem *items,  uint16_t array_size, const char *key) {
	uint16_t i;

	for(i = 0; i < array_size; i++) {
		if(!strcasecmp(key, items[i].key))
			return &items[i];
	}
	return NULL;
}

static uint8_t width2mask(int w) {
	return (0xff >> (8-w));
}


int mk2r_get_mok_value(const uint8_t mok_buffer[8], const char *key) {
	const struct citem *cp = find_citem(mok_items, ARRAY_SIZE(mok_items), key);
	if(!cp)
		return -1;

	uint16_t idx = cp->off + cp->base_bit / 8;
	uint16_t bit = cp->base_bit % 8;
	uint16_t mask = width2mask(cp->width);

	if(idx >= 8) {
		err("(mhstates) %s() idx %d out of range!", __func__, idx);
		return -1;
	}

	return (mok_buffer[idx] >> (bit + 1 - cp->width)) & mask;
}

int mk2_get_mpk_value(const uint8_t mpk_buffer[4], const char *key) {
	const struct citem *cp = find_citem(mpk_items, ARRAY_SIZE(mpk_items), key);
	if(!cp)
		return -1;

	uint16_t idx = cp->off + cp->base_bit / 8;
	uint16_t bit = cp->base_bit % 8;
	uint16_t mask = width2mask(cp->width);

	if(idx >= 4) {
		err("(mhstates) %s() idx %d out of range!", __func__, idx);
		return -1;
	}

	return (mpk_buffer[idx] >> (bit + 1 - cp->width)) & mask;
}


void mk2r_debug_print_mok_values(const uint8_t mok_buffer[8]) {
	uint16_t i;

	if(log_get_level() < LOGSV_DBG1)
		return;

	for(i = 0; i < ARRAY_SIZE(mok_items); i++) {
		dbg1("(mhstates) mok %s: %d", mok_items[i].key, mk2r_get_mok_value(mok_buffer, mok_items[i].key));
	}
}

void mk2_debug_print_mpk_values(const uint8_t mpk_buffer[4]) {
	uint16_t i;

	if(log_get_level() < LOGSV_DBG1)
		return;

	for(i = 0; i < ARRAY_SIZE(mpk_items); i++) {
		dbg1("(mhstates) mpk %s: %d", mpk_items[i].key, mk2_get_mpk_value(mpk_buffer, mpk_items[i].key));
	}
}
