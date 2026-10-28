/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Stubs for logging, app context, event bus and mhcontrol, shared by the tests
 * of src/cat_native.c (native_test.c, router_test.c), plus test helpers.
 */

#ifndef CAT_TEST_STUBS_H
#define CAT_TEST_STUBS_H

#include <ev.h>
#include <stdint.h>
#include <stdio.h>
#include "eventbus.h"
#include "mhcontrol.h"

#define RIG_IC7300 0x88
#define RIG_K3     0x62
#define RIG_TS590  0x7D
#define RIG_FT817  0x29	// older Yaesu, binary protocol
#define RIG_FT767  0x53	// no decoder

extern struct ev_loop *loop;
extern int failures;

#define CHECK(cond, ...) do { \
	if(!(cond)) { \
		printf("FAIL %s:%d: ", __FILE__, __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
		failures++; \
	} \
} while(0)

// mhc_get_speed_params() returns speed if speed_valid.
extern struct mhc_speed_cfg speed;
extern int speed_valid;
// mhc_get_state()
extern uint8_t keyer_state_val;

// mhc_update_radio_info() calls, the last MAX_PUB
#define MAX_PUB 64
extern struct mhc_radio_info pub[MAX_PUB];
extern int npub;
const struct mhc_radio_info *last_pub(void);

// EV_CAT_MONITOR events
extern int mon_lines;
extern char mon_last[256];

// EV_KEYER_STATE subscriber
extern event_cb_fn keyer_state_cb;
void keyer_state(int state);

void run_for(double s);
size_t parse_hex(const char *s, uint8_t *buf);

// The keyer side of the microHAM protocol (src/mux.c, src/demux.c) for channel R1: a byte to
// the host is a sequence of 4, the sync byte with its MSB, then the payload.
void keyer_r1_encode(uint8_t b, uint8_t f[4]);
// Host sequences, byte by byte: returns 1 when c completes a channel R1 byte (*b).
struct keyer_rx {
	int sync, idx;
};
int keyer_r1_decode(struct keyer_rx *k, uint8_t c, uint8_t *b);

#endif // CAT_TEST_STUBS_H
