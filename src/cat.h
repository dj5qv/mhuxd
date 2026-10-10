/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Types shared by the native CAT decoders (cat_civ.c, cat_ascii.c), the CAT
 * monitor and the tests. See docs/task_native_cat.md.
 */

#ifndef CAT_H
#define CAT_H

#include <stddef.h>
#include <stdint.h>

// Direction of CAT bytes. A trace tags them H1..H9 (client), M (mhuxd), R (radio).
enum cat_dir {
	CAT_DIR_HOST,	// client application -> radio
	CAT_DIR_OWN,	// mhuxd's own query -> radio
	CAT_DIR_RADIO,	// radio -> host (on Icom this includes the bus echo)
};

enum cat_frame_kind {
	CAT_FRAME_CMD,		// controller -> radio
	CAT_FRAME_ECHO,		// CI-V bus echo of a command already seen as host data
	CAT_FRAME_REPLY,	// radio -> controller, answers a pending query
	CAT_FRAME_ACK,		// CI-V FB, radio accepted a command
	CAT_FRAME_NAK,		// CI-V FA, radio rejected a command
	CAT_FRAME_UNSOLICITED,	// CI-V transceive, auto information
	CAT_FRAME_UNMATCHED,	// CI-V radio -> controller, no pending query fits
	CAT_FRAME_FOREIGN,	// CI-V neither from nor to the radio
	CAT_FRAME_BAD,		// layout or values don't fit the command, ignored
	CAT_FRAME_JAM,		// CI-V collision code FC
	CAT_FRAME_DROPPED,	// incomplete or overlong frame
	CAT_FRAME_ERROR,	// ASCII error reply (?; E; O;)
};

struct cat_frame {
	enum cat_frame_kind kind;
	enum cat_dir dir;
	uint64_t t_ms;
	const uint8_t *buf;	// whole frame
	size_t len;
	int applied;		// the frame updated cat_state
	int text;		// ASCII protocol: shown as quoted text in traces
	int own;		// part of mhuxd's own query: the query, its echo, reply or acknowledge
};

typedef void (*cat_frame_cb)(const struct cat_frame *frame, void *user_data);

struct cat_stats {
	uint32_t frames;
	uint32_t replies;
	uint32_t applied;
	uint32_t unmatched;
	uint32_t bad;
	uint32_t jams;
	uint32_t dropped;
	uint32_t foreign;
	uint32_t errors;
	uint32_t own;		// radio frames answering mhuxd's own queries (echo, reply, ack, error)
};

// Commands waiting for their answer (cat_civ_pending(), cat_ascii_pending()).
enum {
	CAT_PENDING_CLIENT = 1,	// from a client, or a client frame partly received
	CAT_PENDING_OWN = 2,	// mhuxd's own query
};

const char *cat_frame_kind_str(enum cat_frame_kind kind);

#endif // CAT_H
