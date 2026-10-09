/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Icom CI-V framer and decoder for native CAT (docs/task_native_cat.md).
 *
 * Fed with the bytes of each direction, it frames them, pairs radio replies
 * and acknowledges with the commands that caused them, and writes decoded
 * frequencies, modes and split into a struct cat_state. It only listens.
 *
 * Behind the keyer the radio's CI-V jack is a single-wire bus, so the radio
 * stream also carries the echo of every command. That alone is enough to
 * decode: replies are only accepted for a query seen before (as host data or
 * as echo), sets only once the radio acknowledged them with FB.
 */

#ifndef CAT_CIV_H
#define CAT_CIV_H

#include <stddef.h>
#include <stdint.h>
#include "cat_state.h"

#define CIV_MAX_FRAME 64	// longer frames (scope data, memory dumps) are dropped

struct cat_civ_cfg {
	uint8_t radio_addr;	// 0 = learn from the traffic
	uint8_t freq_len;	// BCD frequency bytes, 0 = 5 (4 on IC-731/IC-735)
};

enum cat_civ_kind {
	CIV_CMD,	// controller -> radio
	CIV_ECHO,	// bus echo of a command already seen as host data
	CIV_REPLY,	// radio -> controller, answers a pending query
	CIV_ACK,	// FB, radio accepted a command
	CIV_NAK,	// FA, radio rejected a command
	CIV_UNSOLICITED,// radio -> 00 (transceive)
	CIV_UNMATCHED,	// radio -> controller, no pending query fits
	CIV_FOREIGN,	// neither from nor to the radio
	CIV_BAD,	// layout or values don't fit the command, ignored
	CIV_JAM,	// collision code FC
	CIV_DROPPED,	// incomplete or overlong frame
};

struct cat_civ_frame {
	enum cat_civ_kind kind;
	enum cat_dir dir;
	uint64_t t_ms;
	const uint8_t *buf;	// whole frame, FE FE ... FD; NULL for JAM
	size_t len;
	int applied;		// the frame updated cat_state
};

typedef void (*cat_civ_frame_cb)(const struct cat_civ_frame *frame, void *user_data);

struct cat_civ_stats {
	uint32_t frames;
	uint32_t replies;
	uint32_t applied;
	uint32_t unmatched;
	uint32_t bad;
	uint32_t jams;
	uint32_t dropped;
	uint32_t foreign;
};

struct cat_civ;

struct cat_civ *cat_civ_create(const struct cat_civ_cfg *cfg, struct cat_state *st);
void cat_civ_destroy(struct cat_civ *civ);
// Called for every frame, for the CAT monitor and tests.
void cat_civ_set_frame_cb(struct cat_civ *civ, cat_civ_frame_cb cb, void *user_data);
void cat_civ_feed(struct cat_civ *civ, enum cat_dir dir, const uint8_t *buf, size_t len, uint64_t t_ms);
uint8_t cat_civ_radio_addr(const struct cat_civ *civ);
const struct cat_civ_stats *cat_civ_get_stats(const struct cat_civ *civ);
const char *cat_civ_kind_str(enum cat_civ_kind kind);

#endif // CAT_CIV_H
