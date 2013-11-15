/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include "mhmk2.h"
#include "util.h"
#include "logger.h"
#include "citem.h"

struct citem mpk_items[] = {
	CITEM("lineUpstream", 0, 0, 1 ),
	CITEM("micUpstream", 0, 1, 1 ),
	CITEM("downstream", 0, 2, 1 ),
	CITEM("audioCToAForced", 0, 3, 1 ),
	CITEM("audioAToCForced", 0, 4, 1 ),
	CITEM("frontMicSelected", 0, 5, 1 ),

	CITEM("sysPwrVoltage", 1, 7, 8 ),
	CITEM("steppirVer", 2, 7, 8 ),
	CITEM("steppirVerHi", 3, 7, 8 ),
};

int mk2_get_mpk_value(const uint8_t mpk_buffer[4], const char *key) {
	return citem_get_value(mpk_items, ARRAY_SIZE(mpk_items), mpk_buffer, 4, key);
}

void mk2_debug_print_mpk_values(const uint8_t mpk_buffer[4]) {
	citem_debug_print_values("mpk", mpk_items, ARRAY_SIZE(mpk_items), mpk_buffer, 4);
}

