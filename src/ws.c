/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#include <ev.h>
#include "ws.h"
#include "http_server.h"
#include "http_server_int.h"
#include "logger.h"

#define MOD_ID "ws"

/* ------------------------------------------------------------------ */
/*  SHA-1 (RFC 3174) – used only for WebSocket handshake accept key   */
/* ------------------------------------------------------------------ */

struct sha1_ctx {
	uint32_t h[5];
	uint64_t length;
	uint8_t block[64];
	size_t block_len;
};

static uint32_t rol32(uint32_t v, uint8_t shift) {
	return (v << shift) | (v >> (32 - shift));
}

static void sha1_init(struct sha1_ctx *ctx) {
	ctx->h[0] = 0x67452301;
	ctx->h[1] = 0xEFCDAB89;
	ctx->h[2] = 0x98BADCFE;
	ctx->h[3] = 0x10325476;
	ctx->h[4] = 0xC3D2E1F0;
	ctx->length = 0;
	ctx->block_len = 0;
}

static void sha1_transform(struct sha1_ctx *ctx, const uint8_t block[64]) {
	uint32_t w[80];
	for(int i = 0; i < 16; i++) {
		w[i] = ((uint32_t)block[i * 4] << 24) |
			((uint32_t)block[i * 4 + 1] << 16) |
			((uint32_t)block[i * 4 + 2] << 8) |
			(uint32_t)block[i * 4 + 3];
	}
	for(int i = 16; i < 80; i++)
		w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

	uint32_t a = ctx->h[0];
	uint32_t b = ctx->h[1];
	uint32_t c = ctx->h[2];
	uint32_t d = ctx->h[3];
	uint32_t e = ctx->h[4];

	for(int i = 0; i < 80; i++) {
		uint32_t f;
		uint32_t k;
		if(i < 20) {
			f = (b & c) | ((~b) & d);
			k = 0x5A827999;
		} else if(i < 40) {
			f = b ^ c ^ d;
			k = 0x6ED9EBA1;
		} else if(i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8F1BBCDC;
		} else {
			f = b ^ c ^ d;
			k = 0xCA62C1D6;
		}
		uint32_t temp = rol32(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = rol32(b, 30);
		b = a;
		a = temp;
	}

	ctx->h[0] += a;
	ctx->h[1] += b;
	ctx->h[2] += c;
	ctx->h[3] += d;
	ctx->h[4] += e;
}

static void sha1_update(struct sha1_ctx *ctx, const uint8_t *data, size_t len) {
	ctx->length += (uint64_t)len * 8;
	while(len > 0) {
		size_t copy_len = 64 - ctx->block_len;
		if(copy_len > len)
			copy_len = len;
		memcpy(ctx->block + ctx->block_len, data, copy_len);
		ctx->block_len += copy_len;
		data += copy_len;
		len -= copy_len;
		if(ctx->block_len == 64) {
			sha1_transform(ctx, ctx->block);
			ctx->block_len = 0;
		}
	}
}

static void sha1_final(struct sha1_ctx *ctx, uint8_t out[20]) {
	ctx->block[ctx->block_len++] = 0x80;
	if(ctx->block_len > 56) {
		while(ctx->block_len < 64)
			ctx->block[ctx->block_len++] = 0;
		sha1_transform(ctx, ctx->block);
		ctx->block_len = 0;
	}
	while(ctx->block_len < 56)
		ctx->block[ctx->block_len++] = 0;

	for(int i = 7; i >= 0; i--)
		ctx->block[ctx->block_len++] = (ctx->length >> (i * 8)) & 0xff;

	sha1_transform(ctx, ctx->block);

	for(int i = 0; i < 5; i++) {
		out[i * 4] = (ctx->h[i] >> 24) & 0xff;
		out[i * 4 + 1] = (ctx->h[i] >> 16) & 0xff;
		out[i * 4 + 2] = (ctx->h[i] >> 8) & 0xff;
		out[i * 4 + 3] = ctx->h[i] & 0xff;
	}
}

/* ------------------------------------------------------------------ */
/*  Base64 encoding                                                   */
/* ------------------------------------------------------------------ */

static int base64_encode(const uint8_t *src, size_t src_len, char *dst, size_t dst_size) {
	static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t out_len = ((src_len + 2) / 3) * 4;
	if(dst_size < out_len + 1)
		return -1;

	size_t j = 0;
	for(size_t i = 0; i < src_len; i += 3) {
		uint32_t v = (uint32_t)src[i] << 16;
		if(i + 1 < src_len)
			v |= (uint32_t)src[i + 1] << 8;
		if(i + 2 < src_len)
			v |= src[i + 2];

		dst[j++] = table[(v >> 18) & 0x3f];
		dst[j++] = table[(v >> 12) & 0x3f];
		dst[j++] = (i + 1 < src_len) ? table[(v >> 6) & 0x3f] : '=';
		dst[j++] = (i + 2 < src_len) ? table[v & 0x3f] : '=';
	}

	dst[j] = 0x00;
	return (int)j;
}

/* ------------------------------------------------------------------ */
/*  UTF-8 validation (RFC 3629)                                       */
/* ------------------------------------------------------------------ */

static int ws_validate_utf8(const uint8_t *data, size_t len) {
	size_t i = 0;
	while(i < len) {
		uint8_t c = data[i];
		if(c <= 0x7f) {
			i++;
			continue;
		}

		if((c & 0xe0) == 0xc0) {
			if(i + 1 >= len)
				return 0;
			uint8_t c1 = data[i + 1];
			if((c1 & 0xc0) != 0x80)
				return 0;
			uint32_t cp = ((uint32_t)(c & 0x1f) << 6) | (uint32_t)(c1 & 0x3f);
			if(cp < 0x80)
				return 0;
			i += 2;
			continue;
		}

		if((c & 0xf0) == 0xe0) {
			if(i + 2 >= len)
				return 0;
			uint8_t c1 = data[i + 1];
			uint8_t c2 = data[i + 2];
			if((c1 & 0xc0) != 0x80 || (c2 & 0xc0) != 0x80)
				return 0;
			uint32_t cp = ((uint32_t)(c & 0x0f) << 12) |
				((uint32_t)(c1 & 0x3f) << 6) |
				(uint32_t)(c2 & 0x3f);
			if(cp < 0x800)
				return 0;
			if(cp >= 0xd800 && cp <= 0xdfff)
				return 0;
			i += 3;
			continue;
		}

		if((c & 0xf8) == 0xf0) {
			if(i + 3 >= len)
				return 0;
			uint8_t c1 = data[i + 1];
			uint8_t c2 = data[i + 2];
			uint8_t c3 = data[i + 3];
			if((c1 & 0xc0) != 0x80 || (c2 & 0xc0) != 0x80 || (c3 & 0xc0) != 0x80)
				return 0;
			uint32_t cp = ((uint32_t)(c & 0x07) << 18) |
				((uint32_t)(c1 & 0x3f) << 12) |
				((uint32_t)(c2 & 0x3f) << 6) |
				(uint32_t)(c3 & 0x3f);
			if(cp < 0x10000 || cp > 0x10ffff)
				return 0;
			i += 4;
			continue;
		}

		return 0;
	}

	return 1;
}

/* ------------------------------------------------------------------ */
/*  WebSocket frame helpers                                           */
/* ------------------------------------------------------------------ */

static int ws_send_frame(struct http_connection *hcon, uint8_t opcode, const char *payload, size_t payload_len) {
	if(!hcon || payload_len > WS_MAX_PAYLOAD_SIZE)
		return -1;

	uint8_t hdr[10];
	size_t hdr_len = 0;
	hdr[hdr_len++] = 0x80 | (opcode & 0x0f);

	if(payload_len < 126) {
		hdr[hdr_len++] = (uint8_t)payload_len;
	} else if(payload_len <= 0xffff) {
		hdr[hdr_len++] = 126;
		hdr[hdr_len++] = (payload_len >> 8) & 0xff;
		hdr[hdr_len++] = payload_len & 0xff;
	} else {
		hdr[hdr_len++] = 127;
		for(int i = 7; i >= 0; i--)
			hdr[hdr_len++] = (payload_len >> (i * 8)) & 0xff;
	}

	if(queue_response_data(hcon, (const char*)hdr, hdr_len) != 0)
		return -1;

	if(payload_len && queue_response_data(hcon, payload, payload_len) != 0)
		return -1;

	return 0;
}

static int ws_append_buf(char **buf, size_t *buf_len, size_t *buf_cap, const char *data, size_t len) {
	if(!len)
		return 0;

	if(*buf_len + len > WS_MAX_PAYLOAD_SIZE)
		return -1;

	if(*buf_cap < *buf_len + len) {
		size_t new_cap = *buf_cap ? *buf_cap : 1024;
		while(new_cap < *buf_len + len)
			new_cap *= 2;
		char *new_buf = realloc(*buf, new_cap);
		if(!new_buf)
			return -1;
		*buf = new_buf;
		*buf_cap = new_cap;
	}

	memcpy(*buf + *buf_len, data, len);
	*buf_len += len;
	return 0;
}

static void ws_reset_frag(struct http_connection *hcon) {
	hcon->ws_frag_active = 0;
	hcon->ws_frag_opcode = 0;
	hcon->ws_frag_len = 0;
}

/* ------------------------------------------------------------------ */
/*  Ping / pong timer                                                 */
/* ------------------------------------------------------------------ */

void ws_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	(void)revents;
	struct http_connection *hcon = w->data;
	if(!hcon || !hcon->ws_enabled || hcon->terminate)
		return;

	ev_tstamp now = ev_now(loop);
	if(hcon->ws_waiting_pong) {
		if(now - hcon->ws_ping_sent_at >= WS_PONG_TIMEOUT) {
			warn("websocket pong timeout");
			if(hs_ws_close(hcon, 1001, "pong timeout") != 0)
				rem_connection(hcon);
		}
		return;
	}

	if(now - hcon->ws_last_rx >= WS_IDLE_PING_INTERVAL) {
		if(ws_send_frame(hcon, WS_OP_PING, NULL, 0) != 0) {
			rem_connection(hcon);
			return;
		}
		hcon->ws_waiting_pong = 1;
		hcon->ws_ping_sent_at = now;
	}
}

/* ------------------------------------------------------------------ */
/*  Incoming raw-data callback (installed via upgraded mode)           */
/* ------------------------------------------------------------------ */

int ws_raw_data_cb(struct http_connection *hcon, const char *data, size_t len, void *user_data) {
	(void)user_data;

	if(!hcon || !len)
		return 0;

	if(hcon->ws_inbuf_len + len > WS_MAX_PAYLOAD_SIZE * 2) {
		warn("websocket receive buffer exceeded");
		return -1;
	}

	if(hcon->ws_inbuf_cap < hcon->ws_inbuf_len + len) {
		size_t new_cap = hcon->ws_inbuf_cap ? hcon->ws_inbuf_cap : 1024;
		while(new_cap < hcon->ws_inbuf_len + len)
			new_cap *= 2;
		char *new_buf = realloc(hcon->ws_inbuf, new_cap);
		if(!new_buf)
			return -1;
		hcon->ws_inbuf = new_buf;
		hcon->ws_inbuf_cap = new_cap;
	}

	memcpy(hcon->ws_inbuf + hcon->ws_inbuf_len, data, len);
	hcon->ws_inbuf_len += len;
	hcon->ws_last_rx = ev_now(hcon->hs->loop);

	size_t off = 0;
	while(hcon->ws_inbuf_len - off >= 2) {
		uint8_t *p = (uint8_t*)hcon->ws_inbuf + off;
		uint8_t b0 = p[0];
		uint8_t b1 = p[1];
		uint8_t fin = (b0 >> 7) & 0x01;
		uint8_t opcode = b0 & 0x0f;
		uint8_t masked = (b1 >> 7) & 0x01;
		uint64_t payload_len = b1 & 0x7f;
		size_t hdr_len = 2;

		if((b0 & 0x70) != 0) {
			warn("websocket RSV bits not supported");
			return -1;
		}

		if((opcode & 0x08) && !fin) {
			warn("fragmented websocket control frames are not allowed");
			return -1;
		}

		if(payload_len == 126) {
			if(hcon->ws_inbuf_len - off < 4)
				break;
			payload_len = ((uint8_t)p[2] << 8) | (uint8_t)p[3];
			hdr_len += 2;
		} else if(payload_len == 127) {
			if(hcon->ws_inbuf_len - off < 10)
				break;
			payload_len = 0;
			for(int i = 0; i < 8; i++)
				payload_len = (payload_len << 8) | (uint8_t)p[2 + i];
			hdr_len += 8;
		}

		if(!masked) {
			warn("websocket client frame must be masked");
			return -1;
		}

		if(payload_len > WS_MAX_PAYLOAD_SIZE) {
			warn("websocket payload too large");
			return -1;
		}

		if(hcon->ws_inbuf_len - off < hdr_len + 4 + payload_len)
			break;

		uint8_t *mask = p + hdr_len;
		uint8_t *payload = p + hdr_len + 4;
		for(uint64_t i = 0; i < payload_len; i++)
			payload[i] ^= mask[i & 3];

		if((opcode & 0x08) && payload_len > 125) {
			warn("invalid websocket control frame");
			return -1;
		}

		switch(opcode) {
		case WS_OP_TEXT:
		case WS_OP_BINARY:
			if(hcon->ws_frag_active) {
				warn("received new data frame while fragmented message is active");
				return -1;
			}
			if(fin) {
				if(opcode == WS_OP_TEXT && !ws_validate_utf8((const uint8_t*)payload, (size_t)payload_len)) {
					hs_ws_close(hcon, 1007, "invalid UTF-8");
					return 0;
				}
				if(hcon->ws_msg_cb) {
					if(hcon->ws_msg_cb(hcon, opcode, (const char*)payload, (size_t)payload_len, hcon->ws_user_data))
						return -1;
				}
			} else {
				hcon->ws_frag_active = 1;
				hcon->ws_frag_opcode = opcode;
				hcon->ws_frag_len = 0;
				if(ws_append_buf(&hcon->ws_frag_buf, &hcon->ws_frag_len, &hcon->ws_frag_cap,
					(const char*)payload, (size_t)payload_len) != 0)
					return -1;
			}
			break;
		case WS_OP_CONT:
			if(!hcon->ws_frag_active) {
				warn("unexpected websocket continuation frame");
				return -1;
			}
			if(ws_append_buf(&hcon->ws_frag_buf, &hcon->ws_frag_len, &hcon->ws_frag_cap,
				(const char*)payload, (size_t)payload_len) != 0)
				return -1;
			if(fin) {
				if(hcon->ws_frag_opcode == WS_OP_TEXT &&
				   !ws_validate_utf8((const uint8_t*)hcon->ws_frag_buf, hcon->ws_frag_len)) {
					hs_ws_close(hcon, 1007, "invalid UTF-8");
					ws_reset_frag(hcon);
					return 0;
				}
				if(hcon->ws_msg_cb) {
					if(hcon->ws_msg_cb(hcon, hcon->ws_frag_opcode, hcon->ws_frag_buf,
						hcon->ws_frag_len, hcon->ws_user_data))
						return -1;
				}
				ws_reset_frag(hcon);
			}
			break;
		case WS_OP_PING:
			if(ws_send_frame(hcon, WS_OP_PONG, (const char*)payload, (size_t)payload_len) != 0)
				return -1;
			break;
		case WS_OP_PONG:
			hcon->ws_waiting_pong = 0;
			break;
		case WS_OP_CLOSE:
			if(ws_send_frame(hcon, WS_OP_CLOSE, (const char*)payload, (size_t)payload_len) != 0)
				return -1;
			hcon->terminate = 1;
			ev_io_stop(hcon->hs->loop, &hcon->w_in);
			off += hdr_len + 4 + payload_len;
			goto done;
		default:
			warn("unsupported websocket opcode %u", opcode);
			return -1;
		}

		off += hdr_len + 4 + payload_len;
	}
done:

	if(off > 0) {
		size_t rem = hcon->ws_inbuf_len - off;
		if(rem)
			memmove(hcon->ws_inbuf, hcon->ws_inbuf + off, rem);
		hcon->ws_inbuf_len = rem;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

int hs_ws_upgrade(struct http_connection *hcon, hcon_ws_msg_cb cb, void *user_data) {
	if(!hcon)
		return -1;

	if(hs_get_method(hcon) != HS_HTTP_GET)
		return -1;

	const char *upgrade = hs_get_req_header(hcon, "Upgrade");
	const char *connection = hs_get_req_header(hcon, "Connection");
	const char *key = hs_get_req_header(hcon, "Sec-WebSocket-Key");
	const char *version = hs_get_req_header(hcon, "Sec-WebSocket-Version");

	if(!upgrade || !connection || !key || !version)
		return -1;

	if(strcasecmp(upgrade, "websocket"))
		return -1;

	if(!strcasestr(connection, "upgrade"))
		return -1;

	if(strcmp(version, "13"))
		return -1;

	char accept_src[256];
	if(strlen(key) + 36 >= sizeof(accept_src))
		return -1;

	snprintf(accept_src, sizeof(accept_src), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

	uint8_t digest[20];
	struct sha1_ctx sha1;
	sha1_init(&sha1);
	sha1_update(&sha1, (const uint8_t*)accept_src, strlen(accept_src));
	sha1_final(&sha1, digest);

	char accept_b64[64];
	if(base64_encode(digest, sizeof(digest), accept_b64, sizeof(accept_b64)) < 0)
		return -1;

	hcon->ws_msg_cb = cb;
	hcon->ws_user_data = user_data;
	hcon->ws_enabled = 1;
	hcon->ws_waiting_pong = 0;
	hcon->ws_last_rx = ev_now(hcon->hs->loop);
	hcon->ws_ping_sent_at = 0;
	ws_reset_frag(hcon);
	ev_timer_init(&hcon->ws_timer, ws_timer_cb, 1.0, 1.0);
	hcon->ws_timer.data = hcon;
	ev_timer_start(hcon->hs->loop, &hcon->ws_timer);

	hs_add_rsp_header(hcon, "Upgrade", "websocket");
	hs_add_rsp_header(hcon, "Connection", "Upgrade");
	hs_add_rsp_header(hcon, "Sec-WebSocket-Accept", accept_b64);
	hs_send_headers(hcon, 101, "application/octet-stream");

	return hs_set_upgraded_mode(hcon, ws_raw_data_cb, hcon);
}

int hs_ws_send_text(struct http_connection *hcon, const char *data, size_t len) {
	if(!data)
		return -1;
	return ws_send_frame(hcon, WS_OP_TEXT, data, len);
}

int hs_ws_send_binary(struct http_connection *hcon, const char *data, size_t len) {
	if(!data)
		return -1;
	return ws_send_frame(hcon, WS_OP_BINARY, data, len);
}

int hs_ws_send_pong(struct http_connection *hcon, const char *data, size_t len) {
	if(!data && len)
		return -1;
	return ws_send_frame(hcon, WS_OP_PONG, data, len);
}

int hs_ws_close(struct http_connection *hcon, uint16_t code, const char *reason) {
	if(!hcon)
		return -1;

	if(hcon->terminate)
		return 0;

	ev_io_stop(hcon->hs->loop, &hcon->w_in);

	char payload[127];
	size_t reason_len = reason ? strlen(reason) : 0;
	if(reason_len > 123)
		reason_len = 123;

	payload[0] = (code >> 8) & 0xff;
	payload[1] = code & 0xff;
	if(reason_len)
		memcpy(payload + 2, reason, reason_len);

	if(ws_send_frame(hcon, WS_OP_CLOSE, payload, reason_len + 2) != 0) {
		hcon->terminate = 1;
		return -1;
	}
	hcon->terminate = 1;
	return 0;
}
