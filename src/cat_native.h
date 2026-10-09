/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Native CAT rig mode sync (docs/task_native_cat.md), the alternative to
 * rigctld_client.c. Phase 1: listens to the CAT channel of one radio in both
 * directions, decodes frequency and mode and feeds mhc_update_radio_info().
 * It never sends anything to the radio.
 */

#ifndef CAT_NATIVE_H
#define CAT_NATIVE_H

#include <stdint.h>

struct app_ctx;
struct device;
struct cat_native;

enum {
	CAT_NATIVE_PROTO_AUTO,	// from the channel's rig type
	CAT_NATIVE_PROTO_CIV,
};

struct cat_native_cfg {
	uint8_t radio;		// 1 or 2
	int protocol;		// CAT_NATIVE_PROTO_*
	int civ_address;	// 0 = the channel's icomaddress
	int monitor;		// CAT monitor: trace file + EV_CAT_MONITOR events
	const char *monitor_dir;	// NULL = LOGDIR
};

// Protocol name from the configuration ("auto", "civ"), -1 if unknown.
int cat_native_protocol_from_str(const char *s);

struct cat_native *cat_native_create(struct app_ctx *ctx, struct device *dev, const struct cat_native_cfg *cfg);
void cat_native_destroy(struct cat_native *cn);

#endif // CAT_NATIVE_H
