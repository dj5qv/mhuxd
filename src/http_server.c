/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <unistd.h>
#include <errno.h>
#include <ev.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "config.h"
#include "http_server.h"
#include "http_parser.h"
#include "http_codes.h"
#include "util.h"
#include "pglist.h"
#include "net.h"
#include "logger.h"

#define MOD_ID "http"

#define READ_SIZE (1024)
#define MAX_URL_SIZE (2048)
#define MAX_POST_SIZE (1024*1024)
#define MAX_HEADER_NAME_SIZE (256)
#define MAX_HEADER_VALUE_SIZE (1024)
#define MAX_HEADERS (30)
#define MAX_HEADERS_SIZE (MAX_HEADERS * (MAX_HEADER_NAME_SIZE + 2 + MAX_HEADER_VALUE_SIZE + 2))
#define WS_MAX_PAYLOAD_SIZE (1024 * 1024)
#define WS_IDLE_PING_INTERVAL 30.0
#define WS_PONG_TIMEOUT 15.0

enum {
	WS_OP_CONT = 0x0,
	WS_OP_TEXT = 0x1,
	WS_OP_BINARY = 0x2,
	WS_OP_CLOSE = 0x8,
	WS_OP_PING = 0x9,
	WS_OP_PONG = 0xa
};

enum {
	HEADER_STATE_VALUE,
	HEADER_STATE_FIELD
};

static char *rfc1123_date_time(char buf[30], time_t *t);
static char *rfc1123_current_date_time(char buf[30]);
static int ws_raw_data_cb(struct http_connection *hcon, const char *data, size_t len, void *user_data);
static void ws_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
static void rem_connection(struct http_connection *hcon);
static int ws_validate_utf8(const uint8_t *data, size_t len);

static const char ct_text_html[] = "text/html";
static const char ct_text_css[] = "text/css";
static const char ct_text_js[] = "application/javascript";
static const char ct_app_json[] = "application/json";
static const char ct_image_svg[] = "image/svg+xml";
static const char ct_font_woff[] = "font/woff";
static const char ct_font_woff2[] = "font/woff2";
static const char ct_image_gif[] = "image/gif";
static const char ct_image_png[] = "image/png";


static const char error_page_templ[] =
		"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		"<html><head>\r\n"
		"<title>%d %s</title>\r\n"
		"</head><body>\r\n"
		"<h1>%d %s</h1>\r\n"
		"</body></html>\r\n";

struct header {
	char name[MAX_HEADER_NAME_SIZE + 1];
	char value[MAX_HEADER_VALUE_SIZE + 1];
	uint16_t name_len;
	uint16_t value_len;
};

struct http_directory_map {
	struct PGNode node;
	char url_path[MAX_URL_SIZE+1];
	char *fs_path;
	uint16_t url_path_len;
	uint16_t fs_path_len;
};

struct http_handler {
	struct PGNode node;
	http_handler_func handler_func;
	char path[MAX_URL_SIZE+1];
	uint16_t path_len;
	void *data;
	unsigned int is_dir_handler : 1;
};

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

	hcon_close_cb close_cb;
	void *close_data;
	hcon_data_cb upgraded_data_cb;
	void *upgraded_data;
	hcon_ws_msg_cb ws_msg_cb;
	void *ws_user_data;
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

struct sha1_ctx {
	uint32_t h[5];
	uint64_t length;
	uint8_t block[64];
	size_t block_len;
};

static int queue_response_data(struct http_connection *hcon, const char *data, size_t len) {
	struct http_response *hr;

	if(!len)
		return 0;

	hr = w_calloc(1, sizeof(*hr) + len + 1);
	hr->data = (char*)&hr[1];
	memcpy(hr->data, data, len);
	hr->len = len;
	PG_AddTail(&hcon->response_list, &hr->node);
	ev_io_start(hcon->hs->loop, &hcon->w_out);
	return 0;
}

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

static int split_host_port(const char *host_port, char **host_out, char **port_out) {
	char *tmp = NULL;
	char *host = NULL;
	char *port = NULL;

	if(!host_port || !*host_port)
		return -1;

	tmp = w_strdup(host_port);

	if(tmp[0] == '[') {
		char *end = strchr(tmp, ']');
		if(!end || end[1] != ':' || end[2] == '\0') {
			free(tmp);
			return -1;
		}
		*end = '\0';
		host = w_strdup(tmp + 1);
		port = w_strdup(end + 2);
		free(tmp);
	} else {
		char *sep = strrchr(tmp, ':');
		if(!sep || sep == tmp || sep[1] == '\0') {
			free(tmp);
			return -1;
		}
		*sep = '\0';
		host = w_strdup(tmp);
		port = w_strdup(sep + 1);
		free(tmp);
	}

	*host_out = host;
	*port_out = port;
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

static void ws_reset_frag(struct http_connection *hcon) {
	hcon->ws_frag_active = 0;
	hcon->ws_frag_opcode = 0;
	hcon->ws_frag_len = 0;
}

static void ws_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	(void)revents;
	struct http_connection *hcon = w->data;
	if(!hcon || !hcon->ws_enabled)
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

int serve_dmap(struct http_connection *hcon, const char *fs_path, const char *sub_path) {
	int err;

	dbg1("serve dmap, path: %s file: %s", fs_path, sub_path);

	err = chdir(fs_path);
	if(err) {
		err_e(errno, "Could not change directory to %s !", fs_path);
		switch(errno) {
		case EACCES:
			hs_send_error_page(hcon, 403);
			return 0;
		case ENOENT:
		case ENOTDIR:
			hs_send_error_page(hcon, 404);
			return 0;
		default:
			hs_send_error_page(hcon, 500);
			return 0;
		}
	}

	struct stat st;
	err = stat(sub_path, &st);
	if(err) {
		err_e(errno, "Could not determine file size of %s/%s !", fs_path, sub_path);
		switch(errno) {
		case EACCES:
			hs_send_error_page(hcon, 403);
			return 0;
		case ENOENT:
		case ENOTDIR:
			hs_send_error_page(hcon, 404);
			return 0;
		default:
			hs_send_error_page(hcon, 500);
			return 0;
		}
	}

	if( ! (st.st_mode & S_IFREG) ) {
		err("Requested file is not a file %s/%s !", fs_path, sub_path);
		hs_send_error_page(hcon, 500);
		return 0;
	}

	int fd = open(sub_path, O_RDONLY);

	if(fd < 0) {
		err_e(errno, "Could not open file %s/%s !", fs_path, sub_path);
		switch(errno) {
		case EACCES:
			hs_send_error_page(hcon, 403);
			return 0;
		case ENOENT:
			hs_send_error_page(hcon, 404);
			return 0;
		default:
			hs_send_error_page(hcon, 500);
			return 0;
		}
	}

	char *buf = w_malloc(st.st_size);

	int rsize = read(fd, buf, st.st_size);

	if(rsize != st.st_size) {
		err_e(errno, "Short read, file %s/%s !", fs_path, sub_path);
		hs_send_error_page(hcon, 500);
		free(buf);
		close(fd);
		return 0;
	}

	const char *content_type = ct_text_html;
	if(hcon->url_len >= 4 && !strncasecmp(hcon->url + hcon->url_len - 4, ".gif", 4))
		content_type = ct_image_gif;
	if(hcon->url_len >= 4 && !strncasecmp(hcon->url + hcon->url_len - 4, ".png", 4))
		content_type = ct_image_png;
	if(hcon->url_len >= 4 && !strncasecmp(hcon->url + hcon->url_len - 4, ".css", 4))
		content_type = ct_text_css;
	if(hcon->url_len >= 3 && !strncasecmp(hcon->url + hcon->url_len - 3, ".js", 3))
		content_type = ct_text_js;
	if(hcon->url_len >= 4 && !strncasecmp(hcon->url + hcon->url_len - 4, ".mjs", 4))
		content_type = ct_text_js;
	if(hcon->url_len >= 5 && !strncasecmp(hcon->url + hcon->url_len - 5, ".json", 5))
		content_type = ct_app_json;
	if(hcon->url_len >= 4 && !strncasecmp(hcon->url + hcon->url_len - 4, ".svg", 4))
		content_type = ct_image_svg;
	if(hcon->url_len >= 5 && !strncasecmp(hcon->url + hcon->url_len - 5, ".woff", 5))
		content_type = ct_font_woff;
	if(hcon->url_len >= 6 && !strncasecmp(hcon->url + hcon->url_len - 6, ".woff2", 6))
		content_type = ct_font_woff2;

	hs_send_response(hcon, 200, content_type, buf, st.st_size, &st.st_mtime, 3600);
	free(buf);
	close(fd);

	return 0;
}

static void rem_connection(struct http_connection *hcon) {
	struct http_response *hr;

	if (hcon->close_cb)
		hcon->close_cb(hcon, hcon->close_data);

	ev_io_stop(hcon->hs->loop, &hcon->w_in);
	ev_io_stop(hcon->hs->loop, &hcon->w_out);
	PG_Remove(&hcon->node);

	while((hr = (void*)PG_FIRSTENTRY(&hcon->response_list))) {
		PG_Remove(&hr->node);
		free(hr);
	}

	if(hcon->body)
		free(hcon->body);
	if(hcon->ws_inbuf)
		free(hcon->ws_inbuf);
	if(hcon->ws_frag_buf)
		free(hcon->ws_frag_buf);
	if(hcon->ws_enabled)
		ev_timer_stop(hcon->hs->loop, &hcon->ws_timer);

	close(hcon->fd);
	free(hcon);
}

static int on_message_begin_cb (http_parser *p) {
	struct http_connection *hcon = p->data;

	hcon->response_code = 200;
	hcon->url_len = 0;
	hcon->req_header_idx = 0;
	hcon->req_header_state = HEADER_STATE_FIELD;
	hcon->req_header[0].name_len = 0;
	hcon->req_header[0].value_len = 0;
	hcon->body_len = 0;

	return 0;
}

static int on_h_field_cb (http_parser *p, const char *buf, size_t len) {
	struct http_connection *hcon = p->data;
	uint16_t avail;
	struct header *h = &hcon->req_header[hcon->req_header_idx];

	if(hcon->req_header_idx >= MAX_HEADERS) {
		hcon->response_code = 400;
		return 0;
	}

	switch(hcon->req_header_state) {
	case HEADER_STATE_VALUE:
		hcon->req_header_idx++;
		h++;
		if(hcon->req_header_idx >= MAX_HEADERS) {
			hcon->response_code = 400;
			return 0;
		}
		h->name_len = 0;
		h->value_len = 0;
		hcon->req_header_state = HEADER_STATE_FIELD;
		// fall through

	case HEADER_STATE_FIELD:
		avail = MAX_HEADER_NAME_SIZE - h->name_len;
		if(len > avail) {
			len = avail;
			hcon->response_code = 400;
		}
		memcpy(h->name + h->name_len, buf, len);
		h->name_len += len;
		h->name[h->name_len] = 0x00;
		break;
	}

	return 0;
}

static int on_h_value_cb (http_parser *p, const char *buf, size_t len) {
	struct http_connection *hcon = p->data;
	uint16_t avail;
	struct header *h = &hcon->req_header[hcon->req_header_idx];

	if(hcon->req_header_idx >= MAX_HEADERS) {
		hcon->response_code = 400;
		return 0;
	}

	switch(hcon->req_header_state) {
	case HEADER_STATE_FIELD:
		hcon->req_header_state = HEADER_STATE_VALUE;

	case HEADER_STATE_VALUE:
		avail = MAX_HEADER_VALUE_SIZE - h->value_len;
		if(len > avail) {
			len = avail;
			hcon->response_code = 400;
		}
		memcpy(h->value + h->value_len, buf, len);
		h->value_len += len;
		h->value[h->value_len] = 0x00;
		break;
	}

	return 0;
}


static int on_url_cb (http_parser *p, const char *buf, size_t len) {
	struct http_connection *hcon = p->data;
	uint16_t avail;

	avail = MAX_URL_SIZE - hcon->url_len;
	if(len > avail) {
		len = avail;
		hcon->response_code = 414;
	}

	memcpy(hcon->url + hcon->url_len, buf, len);
	hcon->url_len += len;

	return 0;
}

static int on_body_cb (http_parser *p, const char *buf, size_t len) {
	struct http_connection *hcon = p->data;

	if(!len)
		return 0;

	if(!hcon->body)
		hcon->body = w_malloc(MAX_POST_SIZE + 1);

	uint32_t avail;

	avail = MAX_POST_SIZE - hcon->body_len;
	if(len > avail) {
		len = avail;
		hcon->response_code = 413;
	}

	memcpy(hcon->body + hcon->body_len, buf, len);
	hcon->body_len += len;
	hcon->body[hcon->body_len] = 0x00;

	return 0;
}

static int on_message_complete_cb (http_parser *p) {
	struct http_connection *hcon = p->data;
	struct http_parser_url url;
	struct http_handler *h;
	struct http_directory_map *dm;
	char path[MAX_URL_SIZE + 1];
	char query[MAX_URL_SIZE + 1];
	uint16_t path_len;

	hcon->url[hcon->url_len] = 0x00;

	if(hcon->parser.method != HTTP_GET && hcon->parser.method != HTTP_POST &&
	   hcon->parser.method != HTTP_PUT && hcon->parser.method != HTTP_PATCH &&
	   hcon->parser.method != HTTP_DELETE) {
		warn("Method not implemented: %s", http_method_str(hcon->parser.method));
		hs_send_error_page(hcon, 501);
		return 0;
	}

	if(hcon->response_code != 200) {
		warn("response: %d", hcon->response_code);
		hs_send_error_page(hcon, hcon->response_code);
		return 0;
	}

	memset(&url, 0, sizeof(url));

	if(http_parser_parse_url(hcon->url, hcon->url_len, 0, &url)) {
		warn("URL parse error (%s)", hcon->url);
		hs_send_error_page(hcon, 400);
		return 0;
	}

	memcpy(path, hcon->url + url.field_data[UF_PATH].off, url.field_data[UF_PATH].len);
	path[url.field_data[UF_PATH].len] = 0x00;
	memcpy(query, hcon->url + url.field_data[UF_QUERY].off, url.field_data[UF_QUERY].len);
	query[url.field_data[UF_QUERY].len] = 0x00;
	path_len = url.field_data[UF_PATH].len;

#if 1
	dbg1("%s '%s' ('%s')", http_method_str(hcon->parser.method), path, query);
	// int i;
	// for(i=0; i<=hcon->req_header_idx && i < MAX_HEADERS; i++)
	//	dbg1("header %s: %s", hcon->req_header[i].name, hcon->req_header[i].value);
#endif

	PG_SCANLIST(&hcon->hs->handler_list, h) {
		if(h->is_dir_handler &&  h->path_len <= hcon->url_len &&  !memcmp(h->path, hcon->url, h->path_len)) {
			h->handler_func(hcon, path + h->path_len, query, hcon->body, hcon->body_len, h->data);
			return 0;
		}
		if(!h->is_dir_handler && h->path_len == hcon->url_len && !memcmp(h->path, hcon->url, h->path_len)) {
			h->handler_func(hcon, path, query, hcon->body, hcon->body_len, h->data);
			return 0;
		}
	}

	PG_SCANLIST(&hcon->hs->dir_map_list, dm) {
		//dbg1("comp path_len %d url_path_len %d %s -- %s", path_len, dm->url_path_len, dm->url_path, path);
		if(path_len >= dm->url_path_len && !memcmp(dm->url_path, path, dm->url_path_len)) {

			serve_dmap(hcon, dm->fs_path, path + dm->url_path_len);
			return 0;
		}
	}

	hs_send_error_page(hcon, 404);

	return 0;
}

static void hcon_in_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct http_connection *hcon = w->data;
	int r, nparsed;
	char buf[READ_SIZE];

	do {
		r = read(w->fd, buf, READ_SIZE);

		if(r < 0) {
			if(errno != EAGAIN) {
				err_e(errno, "error reading from http connection!");
				rem_connection(hcon);
			}
			return;
		}

		if(hcon->upgraded) {
			if(!r) {
				dbg0("connection closed");
				rem_connection(hcon);
				return;
			}
			if(hcon->upgraded_data_cb) {
				if(hcon->upgraded_data_cb(hcon, buf, (size_t)r, hcon->upgraded_data)) {
					rem_connection(hcon);
					return;
				}
			}
			continue;
		}

		nparsed = http_parser_execute(&hcon->parser, &hcon->settings, buf, r);

		/*
		int i;
		for(i=0; i<r; i++) {
			nparsed = http_parser_execute(&hcon->parser, &hcon->settings, &buf[i], 1);
		}
		*/

		if((nparsed != r && !hcon->parser.upgrade) || HTTP_PARSER_ERRNO(&hcon->parser) != HPE_OK) {
			dbg0("Parser error: %s", http_errno_name(HTTP_PARSER_ERRNO(&hcon->parser)));
			rem_connection(hcon);
			return;
		}

		if(hcon->parser.upgrade) {
			if(!hcon->upgraded) {
				warn("HTTP upgrade requested but no upgraded mode handler installed");
				rem_connection(hcon);
				return;
			}

			if((size_t)nparsed < (size_t)r && hcon->upgraded_data_cb) {
				if(hcon->upgraded_data_cb(hcon, buf + nparsed, (size_t)r - (size_t)nparsed, hcon->upgraded_data)) {
					rem_connection(hcon);
					return;
				}
			}
			continue;
		}

		if(!r) {
			dbg0("connection closed");
			rem_connection(hcon);
			return;
		}

	} while(r > 0);

}

static void hcon_out_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct http_connection *hcon = w->data;
	struct http_response *hr;

	while((hr = (void*)PG_FIRSTENTRY(&hcon->response_list))) {
		int r = write(hcon->fd, hr->data + hr->sent, hr->len - hr->sent);
		if(r < 0) {
			if(errno != EAGAIN) {
				err_e(errno, "error writing to http connection!");
				rem_connection(hcon);
			}
			return;
		}

		hr->sent += r;
		if(hr->sent == hr->len) {
			PG_Remove(&hr->node);
			free(hr);
		}
	}


	if(PG_LISTEMPTY(&hcon->response_list))
		ev_io_stop(hcon->hs->loop, &hcon->w_out);

	if(PG_LISTEMPTY(&hcon->response_list) && hcon->terminate) {
		rem_connection(hcon);
		return;
	}

}

static void lsnr_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	struct http_server *hs = w->data;

	(void)loop; (void)revents;
	int fd;

	fd = net_accept(w->fd);
	if(fd != -1) {
		struct http_connection *hcon = w_calloc(1, sizeof(*hcon));
		PG_NewList(&hcon->response_list);
		hcon->fd = fd;
		hcon->hs = hs;

		// Parser
		http_parser_init(&hcon->parser, HTTP_REQUEST);
		hcon->parser.data = hcon;

		// Parser settings
		hcon->settings.on_url = on_url_cb;
		hcon->settings.on_body = on_body_cb;
		hcon->settings.on_message_begin = on_message_begin_cb;
		hcon->settings.on_message_complete = on_message_complete_cb;
		hcon->settings.on_header_field = on_h_field_cb;
		hcon->settings.on_header_value = on_h_value_cb;

		// Watchers
		ev_io_init(&hcon->w_in, hcon_in_cb, fd, EV_READ);
		ev_io_init(&hcon->w_out, hcon_out_cb, fd, EV_WRITE);
		hcon->w_in.data = hcon;
		hcon->w_out.data = hcon;

		PG_AddTail(&hs->con_list, &hcon->node);
		ev_io_start(hs->loop, &hcon->w_in);
		dbg1("incoming connection");

	} else {
		warn_e(errno, "(cli) accept() failed on http port!");
	}
}


int hs_add_rsp_header(struct http_connection *hcon, const char *name, const char *value) {
	uint16_t nlen, vlen;

	if(hcon->rsp_header_cnt >= MAX_HEADERS) {
		warn("header size exceeded in response!");
		return -1;
	}

	nlen = strlen(name);
	vlen = strlen(value);

	if(nlen > MAX_HEADER_NAME_SIZE) {
		warn("header name length exceeded in response!");
		return -1;
	}

	if(vlen > MAX_HEADER_VALUE_SIZE) {
		warn("header value length exceeded in response!");
		return -1;
	}

	strcpy(hcon->rsp_header[hcon->rsp_header_cnt].name, name);
	hcon->rsp_header[hcon->rsp_header_cnt].name_len = nlen;
	strcpy(hcon->rsp_header[hcon->rsp_header_cnt].value, value);
	hcon->rsp_header[hcon->rsp_header_cnt].value_len = vlen;
	hcon->rsp_header_cnt++;


	return 0;
}

void hs_send_response(struct http_connection *hcon, uint16_t code, const char *content_type, const char *body, size_t len, time_t *last_modified, int max_age) {
	struct http_response *hr;
	struct http_code *c = http_codes;
	int headers_size, i;
	char rfc1123_current_date[30];
	char rfc1123_last_modified[30];

	while(c->code && c->code != code)
		c++;

	if(!c->code)
		return;

	hr = w_calloc(1, sizeof(*hr) + len + MAX_HEADERS_SIZE + 1);
	hr->data = (char*)&hr[1];

	rfc1123_current_date_time(rfc1123_current_date);

	if(last_modified)
		rfc1123_date_time(rfc1123_last_modified, last_modified);
	else
		memcpy(rfc1123_last_modified, rfc1123_current_date, sizeof(rfc1123_last_modified));


	headers_size = snprintf(hr->data, MAX_HEADERS_SIZE,
				"HTTP/%d.%d %d %s\r\n"
				//"Content-Type: text/html; charset=UTF-8\r\n"
				"Content-Type: %s%s\r\n"
				"Content-Length: %zd\r\n"
				"Date: %s\r\n"
				"Last-Modified: %s\r\n"
				"Cache-Control: max-age=%d\r\n"
				"Server: %s\r\n",
				hcon->parser.http_major, hcon->parser.http_minor, code, c->msg,
				content_type, !strncasecmp(content_type, "text", 4) ? "; charset=UTF-8" : "",
				len, rfc1123_current_date, rfc1123_last_modified, max_age, PACKAGE_STRING);

	for(i = 0; i < hcon->rsp_header_cnt; i++) {
		headers_size += snprintf(hr->data + headers_size, MAX_HEADERS_SIZE - headers_size, "%s: %s\r\n",
			 hcon->rsp_header[i].name, hcon->rsp_header[i].value);
	}

	headers_size += snprintf(hr->data + headers_size, MAX_HEADERS_SIZE - headers_size, "\r\n");

	hcon->rsp_header_cnt = 0;
	hcon->rsp_header_size = 0;

	if(headers_size >= MAX_HEADERS_SIZE) {
		err("response headers too large!");
		hs_send_error_page(hcon, 500);
		return;
	}

	memcpy(hr->data + headers_size, body ,len);
	hr->len = headers_size + len;
	PG_AddTail(&hcon->response_list, &hr->node);
	ev_io_start(hcon->hs->loop, &hcon->w_out);
}

void hs_send_error_page(struct http_connection *hcon, uint16_t code) {
	struct http_code *c = http_codes;
	char buf[1024];
	char code_str[128];

	uint16_t len;

	while(c->code && c->code != code)
		c++;

	if(!c->code)
		return;

	snprintf(code_str, sizeof(code_str), "%d %s", code, c->msg);
	len = snprintf(buf, sizeof(buf), error_page_templ, code, c->msg, code, c->msg);

	hs_send_response(hcon, code, ct_text_html, buf, len, NULL, 0);
}

void hs_set_close_cb(struct http_connection *hcon, hcon_close_cb cb, void *user_data) {
	hcon->close_cb = cb;
	hcon->close_data = user_data;
}

void hs_send_headers(struct http_connection *hcon, uint16_t code, const char *content_type) {
	struct http_response *hr;
	struct http_code *c = http_codes;
	char rfc1123_current_date[30];

	while(c->code && c->code != code)
		c++;

	if(!c->code)
		return;

	hr = w_calloc(1, sizeof(*hr) + MAX_HEADERS_SIZE + 1);
	hr->data = (char*)&hr[1];

	rfc1123_current_date_time(rfc1123_current_date);

	int headers_size = snprintf(hr->data, MAX_HEADERS_SIZE,
				"HTTP/%d.%d %d %s\r\n"
				"Content-Type: %s\r\n"
				"Date: %s\r\n"
				"Cache-Control: no-cache\r\n"
				"Connection: keep-alive\r\n"
				"Server: %s\r\n",
				hcon->parser.http_major, hcon->parser.http_minor, code, c->msg,
				content_type, rfc1123_current_date, PACKAGE_STRING);

	for(int i = 0; i < hcon->rsp_header_cnt; i++) {
		headers_size += snprintf(hr->data + headers_size, MAX_HEADERS_SIZE - headers_size, "%s: %s\r\n",
			 hcon->rsp_header[i].name, hcon->rsp_header[i].value);
	}

	headers_size += snprintf(hr->data + headers_size, MAX_HEADERS_SIZE - headers_size, "\r\n");

	hcon->rsp_header_cnt = 0;
	hcon->rsp_header_size = 0;

	if(headers_size >= MAX_HEADERS_SIZE) {
		err("response headers too large!");
		free(hr);
		hs_send_error_page(hcon, 500);
		return;
	}

	hr->len = headers_size;
	PG_AddTail(&hcon->response_list, &hr->node);
	ev_io_start(hcon->hs->loop, &hcon->w_out);
}

void hs_send_chunk(struct http_connection *hcon, const char *data, size_t len) {
	(void)queue_response_data(hcon, data, len);
}

const char *hs_get_req_header(struct http_connection *hcon, const char *name) {
	if(!hcon || !name)
		return NULL;

	for(uint16_t i = 0; i <= hcon->req_header_idx && i < MAX_HEADERS; i++) {
		struct header *h = &hcon->req_header[i];
		if(!h->name_len)
			continue;
		if(!strcasecmp(h->name, name))
			return h->value;
	}

	return NULL;
}

int hs_set_upgraded_mode(struct http_connection *hcon, hcon_data_cb cb, void *user_data) {
	if(!hcon)
		return -1;

	hcon->upgraded = 1;
	hcon->upgraded_data_cb = cb;
	hcon->upgraded_data = user_data;
	return 0;
}

static int ws_raw_data_cb(struct http_connection *hcon, const char *data, size_t len, void *user_data) {
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
			payload[i] ^= mask[i % 4];

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
			break;
		default:
			warn("unsupported websocket opcode %u", opcode);
			return -1;
		}

		off += hdr_len + 4 + payload_len;
	}

	if(off > 0) {
		size_t rem = hcon->ws_inbuf_len - off;
		if(rem)
			memmove(hcon->ws_inbuf, hcon->ws_inbuf + off, rem);
		hcon->ws_inbuf_len = rem;
	}

	return 0;
}

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

	char payload[127];
	size_t reason_len = reason ? strlen(reason) : 0;
	if(reason_len > 123)
		reason_len = 123;

	payload[0] = (code >> 8) & 0xff;
	payload[1] = code & 0xff;
	if(reason_len)
		memcpy(payload + 2, reason, reason_len);

	if(ws_send_frame(hcon, WS_OP_CLOSE, payload, reason_len + 2) != 0)
		return -1;

	hcon->terminate = 1;
	ev_io_stop(hcon->hs->loop, &hcon->w_in);
	return 0;
}

struct http_server *hs_start(struct ev_loop *loop, const char *host_port) {
	struct http_server *hs;
	struct netlsnr *lsnr_v4 = NULL;
	struct netlsnr *lsnr_v6 = NULL;
	char *bind_desc = NULL;

	if(!host_port || !*host_port) {
		err("Could not create listener: empty bind address");
		return NULL;
	}

	if(host_port[0] == '/') {
		lsnr_v4 = net_create_listener(host_port);
		if(lsnr_v4 == NULL) {
			err_e(errno, "Could not create listener!");
			return NULL;
		}
		bind_desc = w_strdup(host_port);
	} else {
		char *host = NULL;
		char *port = NULL;
		if(split_host_port(host_port, &host, &port) != 0) {
			err("Could not parse bind address %s", host_port);
			return NULL;
		}

		if(!strcmp(host, "localhost") || !strcmp(host, "*") || !strcmp(host, "0.0.0.0") || !strcmp(host, "::")) {
			char v4_name[128];
			char v6_name[128];
			int err_v4 = 0;
			int err_v6 = 0;
			const char *host_v4 = (!strcmp(host, "localhost")) ? "127.0.0.1" : "0.0.0.0";
			const char *host_v6 = (!strcmp(host, "localhost")) ? "[::1]" : "[::]";

			if(sizeof(v4_name) <= (size_t)snprintf(v4_name, sizeof(v4_name), "%s:%s", host_v4, port) ||
			   sizeof(v6_name) <= (size_t)snprintf(v6_name, sizeof(v6_name), "%s:%s", host_v6, port)) {
				free(host);
				free(port);
				err("Could not create listener: bind address too long");
				return NULL;
			}

			lsnr_v6 = net_create_listener_ex(v6_name, NET_LSNR_F_IPV6_V6ONLY);
			if(lsnr_v6 == NULL)
				err_v6 = errno;
			lsnr_v4 = net_create_listener(v4_name);
			if(lsnr_v4 == NULL)
				err_v4 = errno;

			if(!lsnr_v4 && !lsnr_v6) {
				errno = err_v6 ? err_v6 : err_v4;
				err_e(errno, "Could not create listeners %s and %s", v6_name, v4_name);
				free(host);
				free(port);
				return NULL;
			}

			if(lsnr_v4 && lsnr_v6)
				bind_desc = w_strdup(!strcmp(host, "localhost") ? host_port : "dual-stack:any");
			else
				bind_desc = w_strdup(!lsnr_v4 ? v6_name : v4_name);
		} else {
			lsnr_v4 = net_create_listener(host_port);
			if(lsnr_v4 == NULL) {
				err_e(errno, "Could not create listener!");
				free(host);
				free(port);
				return NULL;
			}
			bind_desc = w_strdup(host_port);
		}

		free(host);
		free(port);
	}

	hs = w_malloc(sizeof(*hs));
	PG_NewList(&hs->con_list);
	PG_NewList(&hs->handler_list);
	PG_NewList(&hs->dir_map_list);
	hs->loop = loop;
	hs->lsnr_v4 = lsnr_v4;
	hs->lsnr_v6 = lsnr_v6;
	hs->bind_desc = bind_desc;

	if(hs->lsnr_v4) {
		hs->w_lsnr_v4.data = hs;
		ev_io_init(&hs->w_lsnr_v4, lsnr_cb, net_listener_get_fd(hs->lsnr_v4), EV_READ);
		ev_io_start(hs->loop, &hs->w_lsnr_v4);
	}

	if(hs->lsnr_v6) {
		hs->w_lsnr_v6.data = hs;
		ev_io_init(&hs->w_lsnr_v6, lsnr_cb, net_listener_get_fd(hs->lsnr_v6), EV_READ);
		ev_io_start(hs->loop, &hs->w_lsnr_v6);
	}

	if(hs->lsnr_v4 && hs->lsnr_v6)
		dbg0("http server dual-stack listeners active (IPv4+IPv6)");

	info("http server started on %s", hs->bind_desc ? hs->bind_desc : host_port);
	return hs;
}

void hs_stop(struct http_server *hs) {
	struct http_connection *hcon;
	struct http_handler *h;
	struct http_directory_map *dm;

	if(hs->lsnr_v4)
		ev_io_stop(hs->loop, &hs->w_lsnr_v4);
	if(hs->lsnr_v6)
		ev_io_stop(hs->loop, &hs->w_lsnr_v6);

	while((hcon = (void*)PG_FIRSTENTRY(&hs->con_list))) {
		rem_connection(hcon);
	}

	while((h = (void*)PG_FIRSTENTRY(&hs->handler_list))) {
		PG_Remove(&h->node);
		free(h);
	}

	while((dm = (void*)PG_FIRSTENTRY(&hs->dir_map_list))) {
		PG_Remove(&dm->node);
		if(dm->fs_path)
			free(dm->fs_path);
		free(dm);
	}

	net_destroy_lsnr(hs->lsnr_v4);
	net_destroy_lsnr(hs->lsnr_v6);
	if(hs->bind_desc)
		free(hs->bind_desc);
	free(hs);

	info("http server stopped");
}

int hs_add_directory_map(struct http_server *hs, const char *url_path, const char *fs_path) {
	if(strlen(url_path) > MAX_URL_SIZE)
		return -1;
	struct http_directory_map *dm = w_malloc(sizeof(*dm));
	strcpy(dm->url_path, url_path);
	dm->fs_path = w_strdup(fs_path);
	dm->url_path_len = strlen(url_path);
	dm->fs_path_len = strlen(fs_path);
	PG_AddTail(&hs->dir_map_list, &dm->node);
	return 0;
}

struct http_handler *hs_register_handler(struct http_server *hs, const char *path, http_handler_func handler_func, void *data) {
	struct http_handler *h;
	int len;

	if(!path)
		return NULL;

	len = strlen(path);

	if(len > MAX_URL_SIZE || !len) {
		err("%s path too long or empty (%s)", __func__, path);
		return NULL;
	}

	h = w_calloc(1, sizeof(*h));
	h->handler_func = handler_func;
	strcpy(h->path, path);
	h->path_len = len;
	h->data = data;
	h->is_dir_handler = (path[len - 1] == '/');
	if(!strcmp(path, "/"))
		h->is_dir_handler = 0;
	PG_AddTail(&hs->handler_list, &h->node);

	return h;
}

void hs_unregister_handler(struct http_server *hs, struct http_handler *h) {
	if(!h)
		return;
	struct http_handler *search;
	PG_SCANLIST(&hs->handler_list, search) {
		if(search == h) {
			PG_Remove(&h->node);
			free(h);
			return;
		}
	}
	warn("%s handler not found!", __func__);
}

int16_t hs_get_method(struct http_connection *hcon) {
	int16_t method = HS_HTTP_UNKNOWN;

	if(!hcon)
		return -1;

	switch(hcon->parser.method) {
		case HTTP_GET: method = HS_HTTP_GET; break;
		case HTTP_POST: method = HS_HTTP_POST; break;
		case HTTP_PUT: method = HS_HTTP_PUT; break;
		case HTTP_PATCH: method = HS_HTTP_PATCH; break;
		case HTTP_DELETE: method = HS_HTTP_DELETE; break;
		case HTTP_HEAD: method = HS_HTTP_HEAD; break;
		case HTTP_OPTIONS: method = HS_HTTP_OPTIONS; break;
	}
	return method;
}

const char *hs_method_str(int16_t method) {
	switch(method) {
		case HS_HTTP_GET: return "GET";
		case HS_HTTP_POST: return "POST";
		case HS_HTTP_PUT: return "PUT";
		case HS_HTTP_PATCH: return "PATCH";
		case HS_HTTP_DELETE: return "DELETE";
		case HS_HTTP_HEAD: return "HEAD";
		case HS_HTTP_OPTIONS: return "OPTIONS";
		default: return "UNKNOWN";
	}
}

static const char *DAY_NAMES[] =
  { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *MONTH_NAMES[] =
  { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


static char *rfc1123_date_time(char buf[30], time_t *t) {
    struct tm tm;
    gmtime_r(t, &tm);
    strftime(buf, 30, "---, %d --- %Y %H:%M:%S GMT", &tm);
    memcpy(buf, DAY_NAMES[tm.tm_wday], 3);
    memcpy(buf+8, MONTH_NAMES[tm.tm_mon], 3);
    return buf;
}

static char *rfc1123_current_date_time(char buf[30])
{
    time_t t;
    time(&t);
    return rfc1123_date_time(buf, &t);
}
