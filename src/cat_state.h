/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Normalized radio state decoded from CAT traffic (native CAT, see
 * docs/task_native_cat.md). Pure code: no libev, no I/O, time is passed in
 * by the caller, so it runs unchanged in mhuxd and in tests/cat.
 */

#ifndef CAT_STATE_H
#define CAT_STATE_H

#include <stdint.h>
#include "cat.h"

struct mhc_radio_info;

// Normalized modes, named like the rigctld mode tokens.
enum cat_mode {
	CAT_MODE_NONE = 0,	// unknown / not decoded
	CAT_MODE_LSB,
	CAT_MODE_USB,
	CAT_MODE_CW,
	CAT_MODE_CWR,
	CAT_MODE_RTTY,
	CAT_MODE_RTTYR,
	CAT_MODE_AM,
	CAT_MODE_AMS,
	CAT_MODE_FM,
	CAT_MODE_WFM,
	CAT_MODE_PKTLSB,
	CAT_MODE_PKTUSB,
	CAT_MODE_PKTAM,
	CAT_MODE_PKTFM,
	CAT_MODE_FAX,
	CAT_MODE_PSK,
	CAT_MODE_PSKR,
	CAT_MODE_OTHER,		// decoded, but no keyer mode for it (DV, P25, ...)
	CAT_MODE_MAX
};

const char *cat_mode_token(enum cat_mode mode);
enum cat_mode cat_mode_from_token(const char *token);
// Keyer mode (MOD_CW, ...) or -1 if the keyer mode should not change.
int cat_mode_to_keyer(enum cat_mode mode);
// LSB/USB/AM/FM with the data flag set become PKTLSB/PKTUSB/PKTAM/PKTFM.
enum cat_mode cat_mode_with_data(enum cat_mode base, int data);

// VFO slots as CI-V commands 25/26 address them: selected and unselected VFO.
enum {
	CAT_VFO_SEL = 0,
	CAT_VFO_UNSEL = 1,
};

struct cat_item {
	uint32_t val;
	uint64_t t_ms;		// time of the last update
	uint8_t valid;
};

struct cat_vfo_state {
	struct cat_item freq;	// Hz
	struct cat_item mode;	// base mode (enum cat_mode, never PKT*)
	struct cat_item data;	// data mode flag (USB-D, ...)
};

struct cat_state {
	struct cat_vfo_state vfo[2];	// CAT_VFO_SEL, CAT_VFO_UNSEL
	struct cat_item split;
	struct cat_item vfo_ab;		// 0 = VFO A selected, 1 = VFO B
	uint32_t changes;		// incremented whenever a value changes
};

void cat_state_init(struct cat_state *st);

void cat_state_set_freq(struct cat_state *st, int vfo, uint32_t hz, uint64_t t_ms);
// A changed base mode invalidates the data flag unless the frame carried one.
void cat_state_set_mode(struct cat_state *st, int vfo, enum cat_mode base, uint64_t t_ms);
void cat_state_set_data(struct cat_state *st, int vfo, int data, uint64_t t_ms);
void cat_state_set_split(struct cat_state *st, int split, uint64_t t_ms);
// VFO A (0) or B (1) selected: swaps the slots if the selection changed.
void cat_state_select_vfo(struct cat_state *st, int ab, uint64_t t_ms);
// A<>B: the contents of both VFOs are exchanged.
void cat_state_exchange_vfos(struct cat_state *st, uint64_t t_ms);
// A=B: the unselected VFO gets the selected VFO's contents.
void cat_state_equalize_vfos(struct cat_state *st, uint64_t t_ms);

// Mode of a VFO slot including the data flag (unknown data flag counts as off).
enum cat_mode cat_state_mode(const struct cat_state *st, int vfo);
// 0 if unknown.
uint32_t cat_state_rx_freq(const struct cat_state *st);
uint32_t cat_state_tx_freq(const struct cat_state *st);

// Fills mode and frequencies. mode = -1 if no keyer mode is known.
// Returns 0 if at least the mode or the RX frequency is known, -1 otherwise.
int cat_state_to_radio_info(const struct cat_state *st, uint8_t radio, struct mhc_radio_info *info);

#endif // CAT_STATE_H
