/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include "mhmk2r.h"
#include "util.h"
#include "logger.h"
#include "citem.h"

struct citem mok_items[] = {
	CITEM("txFocus", 0, 0, 1),
	CITEM("rxFocus", 0, 1, 1 ),
	CITEM("stereoFocus", 0, 2, 1 ),
	CITEM("dvkFocus", 0, 3, 1 ),
	CITEM("earsAuto", 0, 4, 1 ),
	CITEM("directControl", 0, 5, 1 ),

	CITEM("lptCw", 0, 8, 1 ),
	CITEM("lptCw", 0, 9, 1 ),
	CITEM("lptTxFocusPin", 0, 10, 1 ),
	CITEM("lptRxFocusPin", 0, 11, 1 ),
	CITEM("lptStereoFocusPin", 0, 12, 1 ),
	CITEM("focusAuto", 0, 13, 1 ),

	CITEM("ears.left.r1Main", 0, 16, 1 ),
	CITEM("ears.left.r1Sub", 0, 17, 1 ),
	CITEM("ears.left.scLeft", 0, 18, 1 ),
	CITEM("ears.left.scRight", 0, 19, 1 ),
	CITEM("ears.left.r2Main", 0, 20, 1 ),
	CITEM("ears.left.r2Sub", 0, 21, 1 ),
	CITEM("wkTxFocus", 0, 22, 1 ),
	CITEM("wkKeyOutFocus", 0, 23, 1 ),

	CITEM("ears.right.r1Main", 0, 24, 1 ),
	CITEM("ears.right.r1Sub", 0, 25, 1 ),
	CITEM("ears.right.scLeft", 0, 26, 1 ),
	CITEM("ears.right.scRight", 0, 27, 1 ),
	CITEM("ears.right.r2Main", 0, 28, 1 ),
	CITEM("ears.right.r2Sub", 0, 29, 1 ),
	CITEM("chanFocus", 0, 30, 1 ),
	CITEM("lptFsIn", 0, 31, 1 ),

	CITEM("a1SteppirVer", 4, 7, 8 ),
	CITEM("a2SteppirVer", 5, 7, 8 ),
	CITEM("a1SteppirVerHi", 6, 7, 8 ),
	CITEM("a2SteppirVerHi", 7, 7, 8 ),

};

struct citem mk2r_hfocus_items[] = {
	CITEM("txFocus", 0, 0, 1 ),
	CITEM("rxFocus", 0, 1, 1 ),
	CITEM("stereoFocus", 0, 2, 1 ),
	CITEM("wkControledByApp", 0, 3, 1 ),
	CITEM("directControl", 0, 4, 1 ),

	CITEM("ears.left.r1Main", 1, 0, 1 ),
	CITEM("ears.left.r1Sub", 1, 1, 1 ),
	CITEM("ears.left.scLeft", 1, 2, 1 ),
	CITEM("ears.left.scRight", 1, 3, 1 ),
	CITEM("ears.left.r2Main", 1, 4, 1 ),
	CITEM("ears.left.r2Sub", 1, 5, 1 ),

	CITEM("ears.right.r1Main", 2, 0, 1 ),
	CITEM("ears.right.r1Sub", 2, 1, 1 ),
	CITEM("ears.right.scLeft", 2, 2, 1 ),
	CITEM("ears.right.scRight", 2, 3, 1 ),
	CITEM("ears.right.r2Main", 2, 4, 1 ),
	CITEM("ears.right.r2Sub", 2, 5, 1 ),
};


int mk2r_get_mok_value(const uint8_t mok_buffer[8], const char *key) {

	return citem_get_value(mok_items, ARRAY_SIZE(mok_items), mok_buffer, 8, key);
}

void mk2r_debug_print_mok_values(const uint8_t mok_buffer[8]) {

	citem_debug_print_values("mok", mok_items, ARRAY_SIZE(mok_items), mok_buffer, 8);
}

int mk2r_set_hfocus_value(uint8_t hfocus_buffer[4], const char *key, int value) {
	dbg1("(mk2r) %s() %s: %d", __func__, key, value);

	return citem_set_value(mk2r_hfocus_items, ARRAY_SIZE(mk2r_hfocus_items), hfocus_buffer, 4, key, value);
}

static const char *copy_keys[] = {
	"txFocus",
	"rxFocus",
	"stereoFocus",
	// "wkControledByApp",
	"directControl",
	"ears.left.r1Main",
	"ears.left.r1Sub",
	"ears.left.scLeft",
	"ears.left.scRight",
	"ears.left.r2Main",
	"ears.left.r2Sub",
	"ears.right.r1Main",
	"ears.right.r1Sub",
	"ears.right.scLeft",
	"ears.right.scRight",
	"ears.right.r2Main",
	"ears.right.r2Sub",
	NULL
};

void mk2r_set_hfocus_from_mok(uint8_t hfocus_buffer[4], const uint8_t mok_buffer[8]) {
	const char **p;

	for(p = copy_keys; *p; p++) {
		mk2r_set_hfocus_value(hfocus_buffer, *p, mk2r_get_mok_value(mok_buffer, *p));
	}
}
