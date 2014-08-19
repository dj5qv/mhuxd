/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include "mhsm.h"
#include "util.h"
#include "logger.h"
#include "citem.h"

struct citem sm_state_items[] = {
	CITEM("curAzimuth", 0, 0, 16),
	CITEM("azimuthOffset", 2, 0, 16 ),
	CITEM("targetAzimuth", 4, 0, 16 ),
	CITEM("portAVolt", 6, 0, 8 ),
	CITEM("portBVolt", 7, 0, 8 ),
};

int sm_get_state_value(const uint8_t mpk_buffer[4], const char *key) {
	return citem_get_value(sm_state_items, ARRAY_SIZE(sm_state_items), mpk_buffer, 9, key);
}

void sm_debug_print_state_values(const uint8_t mpk_buffer[4]) {
	citem_debug_print_values("mpk", sm_state_items, ARRAY_SIZE(sm_state_items), mpk_buffer, 9);
}

