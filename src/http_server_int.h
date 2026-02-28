/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef HTTP_SERVER_INT_H
#define HTTP_SERVER_INT_H

/*
 * Internal definitions shared between http_server.c and ws.c.
 * Not part of the public API — do not include from other modules.
 */

#include <stdint.h>
#include <ev.h>
#include "http_server.h"
#include "http_parser.h"
#include "pglist.h"

#define MAX_URL_SIZE (2048)
#define MAX_POST_SIZE (1024*1024)
#define MAX_HEADER_NAME_SIZE (256)
#define MAX_HEADER_VALUE_SIZE (1024)
#define MAX_HEADERS (30)
#define MAX_HEADERS_SIZE (MAX_HEADERS * (MAX_HEADER_NAME_SIZE + 2 + MAX_HEADER_VALUE_SIZE + 2))

struct header {
	char name[MAX_HEADER_NAME_SIZE + 1];
	char value[MAX_HEADER_VALUE_SIZE + 1];
	uint16_t name_len;
	uint16_t value_len;
};

struct netlsnr;

struct http_server {
	struct PGList con_list;
	struct PGList handler_list;
	struct PGList dir_map_list;
	struct netlsnr *lsnr_v4;
	struct netlsnr *lsnr_v6;
	char *bind_desc;
	struct ev_loop *loop;
	ev_io w_lsnr_v4;
	ev_io w_lsnr_v6;
};

struct http_response {
	struct PGNode node;
	char *data;
	size_t len;
	size_t sent;
};

struct http_connection {
	struct PGNode node;
	struct http_server *hs;
	int fd;
	ev_io w_in, w_out;
	hcon_data_cb upgraded_data_cb;
	void *upgraded_data;
	hcon_close_cb close_cb;
	void *close_data;
	hcon_ws_msg_cb ws_msg_cb;
	void *ws_user_data;

	http_parser parser;
	http_parser_settings settings;
	char url[MAX_URL_SIZE+1];
	uint16_t url_len;

	struct header req_header[MAX_HEADERS];
	uint16_t req_header_state;
	uint16_t req_header_idx;

	struct header rsp_header[MAX_HEADERS];
	uint16_t rsp_header_cnt;
	uint16_t rsp_header_size;

	char *body;
	uint32_t body_len;

	struct PGList response_list;

	uint16_t response_code;
	unsigned int terminate :1;
	unsigned int upgraded :1;
	unsigned int ws_enabled :1;
	unsigned int ws_waiting_pong :1;
	unsigned int ws_frag_active :1;
	unsigned int outq_warned :1;
	size_t outq_bytes;
	size_t outq_peak;
	size_t outq_dropped_bytes;
	uint32_t outq_hard_hits;

	size_t tx_bytes;
	size_t tx_frames;

	char *ws_inbuf;
	size_t ws_inbuf_len;
	size_t ws_inbuf_cap;
	char *ws_frag_buf;
	size_t ws_frag_len;
	size_t ws_frag_cap;
	uint8_t ws_frag_opcode;
	ev_timer ws_timer;
	ev_tstamp ws_last_rx;
	ev_tstamp ws_ping_sent_at;
};

/* Internal functions used by ws.c */
int queue_response_data(struct http_connection *hcon, const char *data, size_t len);
void rem_connection(struct http_connection *hcon);

#endif /* HTTP_SERVER_INT_H */
