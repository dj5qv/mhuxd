/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Native CAT rig mode sync (docs/task_native_cat.md), the alternative to
 * rigctld_client.c. Listens to the CAT channel of one radio in both
 * directions, decodes frequency and mode and feeds mhc_update_radio_info().
 * With own queries on, it asks the radio itself when no application keeps the
 * state fresh, in the gaps of the applications' traffic: their data is held
 * back meanwhile and the answers are taken out of the radio data.
 */

#ifndef CAT_NATIVE_H
#define CAT_NATIVE_H

#include <stdint.h>

struct app_ctx;
struct device;
struct cat_native;

enum {
	CAT_NATIVE_PROTO_AUTO,	// from the channel's rig type
	CAT_NATIVE_PROTO_CIV,	// Icom CI-V
	CAT_NATIVE_PROTO_KENWOOD,	// Kenwood-style ASCII dialects, see cat_ascii.h
	CAT_NATIVE_PROTO_ELECRAFT,
	CAT_NATIVE_PROTO_YAESU,
	CAT_NATIVE_PROTO_FLEX,
	CAT_NATIVE_PROTO_NONE,	// no decoder for the rig type
};

struct cat_native_cfg {
	uint8_t radio;		// 1 or 2
	int protocol;		// CAT_NATIVE_PROTO_*
	int civ_address;	// 0 = the channel's icomaddress
	int own_queries;	// ask the radio when no application does
	int civ_resend;		// CI-V: send commands destroyed in a bus collision again
	int monitor;		// CAT monitor: trace file + EV_CAT_MONITOR events
	const char *monitor_dir;	// NULL = LOGDIR
};

// Protocol name from the configuration ("auto", "civ", "kenwood", "elecraft", "yaesu",
// "flex"), -1 if unknown.
int cat_native_protocol_from_str(const char *s);
const char *cat_native_protocol_str(int protocol);

struct cat_native *cat_native_create(struct app_ctx *ctx, struct device *dev, const struct cat_native_cfg *cfg);
void cat_native_destroy(struct cat_native *cn);

#endif // CAT_NATIVE_H
