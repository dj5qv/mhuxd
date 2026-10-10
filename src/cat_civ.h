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
#include "cat.h"
#include "cat_state.h"

#define CIV_MAX_FRAME 64	// longer frames (scope data, memory dumps) are dropped

struct cat_civ_cfg {
	uint8_t radio_addr;	// 0 = learn from the traffic
	uint8_t freq_len;	// BCD frequency bytes, 0 = 5 (4 on IC-731/IC-735)
};

struct cat_civ;

struct cat_civ *cat_civ_create(const struct cat_civ_cfg *cfg, struct cat_state *st);
void cat_civ_destroy(struct cat_civ *civ);
// Called for every frame, for the CAT monitor and tests.
void cat_civ_set_frame_cb(struct cat_civ *civ, cat_frame_cb cb, void *user_data);
void cat_civ_feed(struct cat_civ *civ, enum cat_dir dir, const uint8_t *buf, size_t len, uint64_t t_ms);
uint8_t cat_civ_radio_addr(const struct cat_civ *civ);
// CAT_PENDING_* flags: commands waiting for their reply or acknowledge.
unsigned cat_civ_pending(const struct cat_civ *civ, uint64_t t_ms);
const struct cat_stats *cat_civ_get_stats(const struct cat_civ *civ);

#endif // CAT_CIV_H
