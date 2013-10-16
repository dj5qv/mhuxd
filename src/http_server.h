#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H 1

#include <stdint.h>
#include <unistd.h>
#include <time.h>

struct http_connection;
struct http_handler;
struct ev_loop;

typedef int (*http_handler_func)(struct http_connection *, const char *path, const char *query,
				 const char *body, uint32_t body_len, void *data);

struct http_server *hs_start(struct ev_loop *loop, const char *host_port);
void hs_stop(struct http_server *hs);

int hs_add_directory_map(struct http_server *hs, const char *url_path, const char *fs_path);
struct http_handler *hs_register_handler(struct http_server *hs, const char *path, http_handler_func handler_func, void *data);
void hs_unregister_handler(struct http_server *hs, struct http_handler *h);

void hs_send_response(struct http_connection *hcon, uint16_t code, const char *content_type, const char *body, size_t len, time_t *, int max_age);
void hs_send_error_page(struct http_connection *hcon, uint16_t code);

int hs_add_rsp_header(struct http_connection *hcon, const char *name, const char *value);

#endif /* HTTP_SERVER_H */
