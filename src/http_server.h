/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H 1

#include <stdint.h>
#include <unistd.h>
#include <time.h>

struct http_connection;
struct http_handler;
struct ev_loop;

enum {
	HS_HTTP_GET,
	HS_HTTP_POST,
	HS_HTTP_PUT,
	HS_HTTP_PATCH,
	HS_HTTP_DELETE,
	HS_HTTP_HEAD,
	HS_HTTP_OPTIONS,
	HS_HTTP_UNKNOWN
};

typedef int (*http_handler_func)(struct http_connection *, const char *path, const char *query,
				 const char *body, uint32_t body_len, void *data);

typedef int (*hcon_data_cb)(struct http_connection *hcon, const char *data, size_t len, void *user_data);
typedef int (*hcon_ws_msg_cb)(struct http_connection *hcon, int opcode, const char *data, size_t len, void *user_data);

struct http_server *hs_start(struct ev_loop *loop, const char *host_port);
void hs_stop(struct http_server *hs);

int hs_add_directory_map(struct http_server *hs, const char *url_path, const char *fs_path);
struct http_handler *hs_register_handler(struct http_server *hs, const char *path, http_handler_func handler_func, void *data);
void hs_unregister_handler(struct http_server *hs, struct http_handler *h);

int16_t hs_get_method(struct http_connection *hcon);
const char *hs_method_str(int16_t method);

void hs_send_response(struct http_connection *hcon, uint16_t code, const char *content_type, const char *body, size_t len, time_t *, int max_age);
void hs_send_error_page(struct http_connection *hcon, uint16_t code);

typedef void (*hcon_close_cb)(struct http_connection *hcon, void *user_data);
void hs_set_close_cb(struct http_connection *hcon, hcon_close_cb cb, void *user_data);

void hs_send_headers(struct http_connection *hcon, uint16_t code, const char *content_type);
void hs_send_chunk(struct http_connection *hcon, const char *data, size_t len);

const char *hs_get_req_header(struct http_connection *hcon, const char *name);
int hs_set_upgraded_mode(struct http_connection *hcon, hcon_data_cb cb, void *user_data);
int hs_ws_upgrade(struct http_connection *hcon, hcon_ws_msg_cb cb, void *user_data);
int hs_ws_send_text(struct http_connection *hcon, const char *data, size_t len);
int hs_ws_send_binary(struct http_connection *hcon, const char *data, size_t len);
int hs_ws_send_pong(struct http_connection *hcon, const char *data, size_t len);
int hs_ws_close(struct http_connection *hcon, uint16_t code, const char *reason);

int hs_add_rsp_header(struct http_connection *hcon, const char *name, const char *value);

#endif /* HTTP_SERVER_H */
