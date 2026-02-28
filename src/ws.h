/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef WS_H
#define WS_H

#include <stdint.h>
#include <stddef.h>
#include <ev.h>

struct http_connection;

enum {
	WS_OP_CONT   = 0x0,
	WS_OP_TEXT   = 0x1,
	WS_OP_BINARY = 0x2,
	WS_OP_CLOSE  = 0x8,
	WS_OP_PING   = 0x9,
	WS_OP_PONG   = 0xa
};

#define WS_MAX_PAYLOAD_SIZE  (1024 * 1024)
#define WS_IDLE_PING_INTERVAL 30.0
#define WS_PONG_TIMEOUT       15.0

/*
 * Internal callbacks installed by ws_upgrade().
 * Not called directly — used as ev/upgrade callbacks.
 */
int ws_raw_data_cb(struct http_connection *hcon, const char *data, size_t len, void *user_data);
void ws_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);

#endif /* WS_H */
