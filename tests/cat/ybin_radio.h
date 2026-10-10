/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Emulated older Yaesu radios with the binary CAT protocol, for the tests of
 * src/cat_ybin.c and the ybin_sim tool. Written apart from the decoder, from
 * what Hamlib's backends send and expect, so Hamlib can check it (ybin_sim
 * with rigctl) and it can check the decoder.
 *
 * The caller feeds the computer's bytes and sends the answers, after the
 * radio's processing time, at the radio's baud rate.
 */

#ifndef YBIN_RADIO_H
#define YBIN_RADIO_H

#include <stddef.h>
#include <stdint.h>

enum ybr_mode {
	YBR_LSB, YBR_USB, YBR_CW, YBR_CWR, YBR_AM, YBR_AMS, YBR_FM, YBR_WFM,
	YBR_RTTY, YBR_RTTYR, YBR_PKTL, YBR_PKTU, YBR_PKTFM, YBR_DIG,
};

struct ybr_state {
	uint32_t freq[2];	// VFO A, B, Hz
	uint8_t mode[2];	// enum ybr_mode
	int vfo;		// 0: A, 1: B
	int split;
	int cat_on;		// FT-847: commands are ignored until CAT on
	unsigned pacing_ms;	// delay after each answer byte
};

struct ybr_stats {
	int commands;		// complete commands
	int answered;		// commands answered
	int unknown;		// commands the radio doesn't know (ignored)
	int partial;		// commands forgotten after a gap
};

struct ybin_radio;

// Model names as in cat_ybin.c: ft757gx2, ft817, ft857, ft897, ft847, ft840, ft890, ft900,
// ft920, ft990, ft1000, ft1000mp. NULL if unknown.
struct ybin_radio *ybr_create(const char *model);
void ybr_destroy(struct ybin_radio *r);
struct ybr_state *ybr_state(struct ybin_radio *r);
const struct ybr_stats *ybr_stats(const struct ybin_radio *r);
// The FT-817 family acknowledges every command; off: like a radio that doesn't.
void ybr_set_ack(struct ybin_radio *r, int on);

// Bytes from the computer at time t (s). Returns the length of the answer copied to out
// (room for 2048 bytes), 0 if none. A command's bytes may come up to 200 ms apart.
size_t ybr_rx(struct ybin_radio *r, uint8_t b, double t, uint8_t *out);

#endif // YBIN_RADIO_H
