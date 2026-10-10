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
#include "cat_ascii.h"
#include "cat_civ.h"
#include "cat_state.h"
#include "cat_ybin.h"

#define CAT_TRACE_LINE_MAX 512

// Keyer mode name (CW, VOICE, FSK, DIGITAL) or "?".
const char *cat_trace_keyer_str(int keyer_mode);

// "rx=14074000 unsel=? mode=PKTUSB split=0 keyer=DIGITAL"
void cat_trace_state(char *buf, size_t sz, const struct cat_state *st);

// "<time> <tag> fe fe ... fd  # <kind>[, applied: <state>]", no newline. Frames of the
// ASCII protocols are written as quoted text: "<time> <tag> \"FA00014074000;\"  # ...".
// Host data is tagged with the client number (H1..H9), radio data R, mhuxd's own data M.
// Radio frames that belong to a client's command name it: "# H2 reply".
void cat_trace_frame(char *buf, size_t sz, const char *time, const struct cat_frame *f,
		     const struct cat_state *st);

// "@civ addr=94[ freq_len=4]"
void cat_trace_civ_config(char *buf, size_t sz, const struct cat_civ_cfg *cfg);
// "@kenwood[ nodata]", "@elecraft", "@yaesu", "@flex"
void cat_trace_ascii_config(char *buf, size_t sz, const struct cat_ascii_cfg *cfg);
// "@ybin model=ft1000mp"
void cat_trace_ybin_config(char *buf, size_t sz, const struct cat_ybin_cfg *cfg);

#endif // CAT_TRACE_H
