/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * CAT trace lines, shared by the CAT monitor (cat_native.c) and the replay
 * tool (tests/cat/cat_replay.c). Format: tests/cat/README.md. Pure code.
 */

#ifndef CAT_TRACE_H
#define CAT_TRACE_H

#include <stddef.h>
#include "cat_civ.h"
#include "cat_state.h"

#define CAT_TRACE_LINE_MAX (16 + 3 * CIV_MAX_FRAME + 160)

// Keyer mode name (CW, VOICE, FSK, DIGITAL) or "?".
const char *cat_trace_keyer_str(int keyer_mode);

// "rx=14074000 unsel=? mode=PKTUSB split=0 keyer=DIGITAL"
void cat_trace_state(char *buf, size_t sz, const struct cat_state *st);

// "<time> <tag> fe fe ... fd  # <kind>[, applied: <state>]", no newline.
// tag is used for host data; radio data is tagged R, mhuxd's own data M.
void cat_trace_civ_frame(char *buf, size_t sz, const char *time, const char *host_tag,
			 const struct cat_civ_frame *f, const struct cat_state *st);

// "@civ addr=94[ freq_len=4]"
void cat_trace_civ_config(char *buf, size_t sz, const struct cat_civ_cfg *cfg);

#endif // CAT_TRACE_H
