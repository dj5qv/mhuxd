/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Decoder for the binary CAT protocol of the older Yaesu radios (FT-817,
 * FT-1000MP, ...), see docs/task_native_cat.md. The computer sends commands of
 * five bytes, four parameters and the opcode last; the radio answers some of
 * them with a block of data whose length depends on the model, the opcode and
 * a parameter, without start or end marker. So the answers are framed by the
 * per-model length tables here, in the order of the commands. No echo, no
 * auto information. Pure code, like cat_civ.c.
 */

#ifndef CAT_YBIN_H
#define CAT_YBIN_H

#include <stddef.h>
#include <stdint.h>
#include "cat.h"
#include "cat_state.h"

#define YBIN_CMD_LEN 5
#define YBIN_MAX_FRAME 80	// longer answers (memory dumps) are passed on in pieces of this size

enum cat_ybin_model {
	CAT_YBIN_FT757GX2,	// FT-757GXII
	CAT_YBIN_FT817,		// FT-817, FT-818
	CAT_YBIN_FT857,
	CAT_YBIN_FT897,
	CAT_YBIN_FT847,
	CAT_YBIN_FT840,
	CAT_YBIN_FT890,
	CAT_YBIN_FT900,
	CAT_YBIN_FT920,
	CAT_YBIN_FT990,
	CAT_YBIN_FT1000,	// FT-1000, FT-1000D
	CAT_YBIN_FT1000MP,	// FT-1000MP, MARK-V, MARK-V Field
	CAT_YBIN_MODEL_NUM
};

struct cat_ybin_cfg {
	enum cat_ybin_model model;
};

// "ft1000mp", ...
const char *cat_ybin_model_str(enum cat_ybin_model model);
// -1 if unknown.
int cat_ybin_model_from_str(const char *s);
// "FT-1000MP", ...
const char *cat_ybin_model_name(enum cat_ybin_model model);

struct cat_ybin;

struct cat_ybin *cat_ybin_create(const struct cat_ybin_cfg *cfg, struct cat_state *st);
void cat_ybin_destroy(struct cat_ybin *y);
// Called for every frame, for the CAT monitor and tests.
void cat_ybin_set_frame_cb(struct cat_ybin *y, cat_frame_cb cb, void *user_data);
// client: 1..CAT_MAX_CLIENTS for CAT_DIR_HOST, ignored otherwise.
void cat_ybin_feed(struct cat_ybin *y, enum cat_dir dir, int client, const uint8_t *buf, size_t len,
		   uint64_t t_ms);
const struct cat_stats *cat_ybin_get_stats(const struct cat_ybin *y);
// CAT_PENDING_* flags: commands waiting for their answer, or keeping the line (see
// YBIN_HOLD_MS).
unsigned cat_ybin_pending(const struct cat_ybin *y, uint64_t t_ms);
// The client has a command partly received, one waiting for its answer, or one that was
// just sent and has no answer.
int cat_ybin_client_busy(const struct cat_ybin *y, int client, uint64_t t_ms);
// Forgets the client's commands waiting for an answer and its partly received command.
void cat_ybin_client_discard(struct cat_ybin *y, int client);
// Without start or end marker, a partly received command and an answer that stops short are
// only noticed after a gap (YBIN_CMD_GAP_MS, YBIN_BYTE_GAP_MS). Their frames come out with the
// next data, or with this call, which the caller makes now and then.
void cat_ybin_expire(struct cat_ybin *y, uint64_t t_ms);

// An answer is coming in (its first bytes are there, the rest is due): returns 1 and whose
// command it answers. Long answers, a slow baud rate or the pacing delay make it take longer
// than other protocols' answers.
int cat_ybin_answering(const struct cat_ybin *y, uint64_t t_ms, int *client, int *own);

// The status query that tells frequency and mode: copies the command to buf (YBIN_CMD_LEN)
// and returns the length of its answer, 0 if the model has none.
size_t cat_ybin_query(const struct cat_ybin *y, uint8_t *buf);

// A command's bytes may come this far apart (the radios wait up to 200 ms).
#define YBIN_CMD_GAP_MS 300
// A command without answer keeps the line this long: the radio needs time for it, Hamlib
// waits up to 80 ms after a command (post_write_delay).
#define YBIN_HOLD_MS 100
// An answer's first byte comes within this time, or the command is given up.
#define YBIN_ANSWER_MS 1000
// An answer whose bytes stop for this long (plus the pacing delay) is over, even if shorter
// than the table says: a memory dump of another firmware version, or a lost byte.
#define YBIN_BYTE_GAP_MS 100

#endif // CAT_YBIN_H
