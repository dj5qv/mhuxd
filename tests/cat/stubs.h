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
#define RIG_FT817  0x29	// legacy Yaesu binary: no decoder yet

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

// mhc_update_radio_info() calls
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

#endif // CAT_TEST_STUBS_H
