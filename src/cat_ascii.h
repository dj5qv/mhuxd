/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Decoder for the Kenwood-style ASCII CAT protocols (Kenwood, Elecraft, newer
 * Yaesu, Flex/PowerSDR), see docs/task_native_cat.md. Frames end with ';',
 * replies name their command and carry the full value, so every well-formed
 * frame from the radio (reply or auto information) updates the state. Client
 * frames that set frequency, mode or data mode are applied as well; the
 * radio sends no acknowledge for them. Pure code, like cat_civ.c.
 */

#ifndef CAT_ASCII_H
#define CAT_ASCII_H

#include <stddef.h>
#include <stdint.h>
#include "cat.h"
#include "cat_state.h"

#define ASCII_MAX_FRAME 64	// longer frames (menus, text) are dropped

enum cat_ascii_dialect {
	CAT_ASCII_KENWOOD,	// Kenwood TS-*
	CAT_ASCII_ELECRAFT,	// K2, K3: data sub modes via DT
	CAT_ASCII_YAESU,	// newer Yaesu ("newcat"): FT-450, FT-950, FT-991, FTdx...
	CAT_ASCII_FLEX,		// Flex, PowerSDR/Thetis (ANAN) and other SDRs emulating Kenwood, ZZ commands
};

struct cat_ascii_cfg {
	enum cat_ascii_dialect dialect;
	int no_data;		// Kenwood: the radio has no data mode (no DA/DT), USB means voice
};

const char *cat_ascii_dialect_str(enum cat_ascii_dialect dialect);
// "kenwood", "elecraft", "yaesu", "flex"; -1 if unknown.
int cat_ascii_dialect_from_str(const char *s);

struct cat_ascii;

struct cat_ascii *cat_ascii_create(const struct cat_ascii_cfg *cfg, struct cat_state *st);
void cat_ascii_destroy(struct cat_ascii *a);
// Called for every frame, for the CAT monitor and tests.
void cat_ascii_set_frame_cb(struct cat_ascii *a, cat_frame_cb cb, void *user_data);
// client: 1..CAT_MAX_CLIENTS for CAT_DIR_HOST, ignored otherwise.
void cat_ascii_feed(struct cat_ascii *a, enum cat_dir dir, int client, const uint8_t *buf, size_t len,
		    uint64_t t_ms);
const struct cat_stats *cat_ascii_get_stats(const struct cat_ascii *a);
// CAT_PENDING_* flags: commands that may still get an answer (see CAT_ASCII_SET_WAIT_MS).
unsigned cat_ascii_pending(const struct cat_ascii *a, uint64_t t_ms);
// A command with parameters may be a set, which gets no answer, or a read (Yaesu "MD0;"): it
// waits for an answer this long only, a query without parameters 2 s.
#define CAT_ASCII_SET_WAIT_MS 200
// The client has a frame partly received or a command waiting for its answer.
int cat_ascii_client_busy(const struct cat_ascii *a, int client, uint64_t t_ms);
// Forgets the client's commands waiting for an answer and its partly received frame.
void cat_ascii_client_discard(struct cat_ascii *a, int client);

#endif // CAT_ASCII_H
