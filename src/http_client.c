/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ev.h>
#include "http_client.h"
#include "http_parser.h"
#include "util.h"
#include "obuf.h"

extern int h_errno;

#define MAX_BUF_LEN 8192
#define BUF_INCREMENT 64

struct http_request {
	struct ev_loop *loop;
	ev_io w;
	int fd;
	http_parser parser;
	http_parser_settings settings;
	struct obuf *obuf;
	int error;
};

static void read_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	struct http_request *req = w->data;
	char buf[1024];
	ssize_t r;
	int nparsed;

	do {
		r = read(req->fd, buf, sizeof(buf));
		fprintf(stderr,"%s() %zd\n", __func__, r);

		if(-1 == r) {
			if(errno == EAGAIN)
				return;
			req->error = errno;
			ev_unloop(req->loop, EVUNLOOP_ONE);
			return;
		}

		if(0 == r) {
			fprintf(stderr, "Connection closed!\n");
			ev_unloop(req->loop, EVUNLOOP_ONE);
			return;
		}

		while((size_t)r > obuf_avail(req->obuf))
			obuf_realloc(req->obuf, req->obuf->capacity + r + 1024);

		memcpy(req->obuf->data + req->obuf->size, buf, r);
		req->obuf->size += r;

		nparsed = http_parser_execute(&req->parser, &req->settings, buf, r);

		if(nparsed != r || HTTP_PARSER_ERRNO(&req->parser) != HPE_OK) {
			req->error = EINVAL;
			ev_unloop(req->loop, EVUNLOOP_ONE);
			return;
		}
	}
	while(r > 0);
}

static int on_message_begin_cb (http_parser *p) {
	fprintf(stderr,"%s()\n", __func__);
	return 0;
}

static int on_headers_complete (http_parser *p) {
	fprintf(stderr,"%s()\n", __func__);
	return 0;
}
static int on_h_field_cb (http_parser *p, const char *buf, size_t len) {
	fprintf(stderr,"%s()\n", __func__);
	return 0;
}
static int on_h_value_cb (http_parser *p, const char *buf, size_t len) {
	fprintf(stderr,"%s()\n", __func__);
	return 0;
}
static int on_url_cb (http_parser *p, const char *buf, size_t len) {
	fprintf(stderr,"%s()\n", __func__);
	return 0;
}
static int on_body_cb (http_parser *p, const char *buf, size_t len) {
	fprintf(stderr,"%s()\n", __func__);
	return 0;
}

static int on_message_complete_cb (http_parser *p) {
	struct http_request *req = p->data;
	ev_unloop(req->loop, EVUNLOOP_ONE);
	fprintf(stderr,"%s()\n", __func__);
	return 0;
}

static const char *create_request_line(const char *url, const struct http_parser_url *pu, const char *method) {
	size_t size = 32;
	char *buf;

	size += strlen(method);

	if(pu->field_set & (1 << UF_HOST))
		size += pu->field_data[UF_HOST].len;

	if(pu->field_set & (1 << UF_PATH))
		size += pu->field_data[UF_PATH].len;
	else
		size += 1;

	if(pu->field_set & (1 << UF_QUERY))
		size += pu->field_data[UF_QUERY].len;

	if(pu->field_set & (1 << UF_FRAGMENT))
		size += (pu->field_data[UF_FRAGMENT].len + 1);

	buf = w_malloc(size);

	strcpy(buf, method);
	strcat(buf, " ");
	if(pu->field_set & (1 << UF_PATH))
		strncat(buf, url + pu->field_data[UF_PATH].off, pu->field_data[UF_PATH].len);
	else
		strcat(buf, "/");

	if(pu->field_set & (1 << UF_QUERY))
		strncat(buf, url + pu->field_data[UF_QUERY].off, pu->field_data[UF_QUERY].len);

	if(pu->field_set & (1 << UF_FRAGMENT)) {
		strcat(buf, "#");
		strncat(buf, url + pu->field_data[UF_FRAGMENT].off, pu->field_data[UF_FRAGMENT].len);
	}

	strcat(buf, " HTTP/1.1\r\nHost: ");
	strncat(buf, url + pu->field_data[UF_HOST].off, pu->field_data[UF_HOST].len);
	strcat(buf, "\r\n\r\n");

	return buf;
}

struct http_request *httpc_get_request(const char *url) {
	char *hostname;
	const char *req_line;
	struct http_request *req;
	struct addrinfo *ai_result, *ai;
	struct http_parser_url pu;
	int sd, rc;
	ssize_t r;

	if(!url || !*url)
		return NULL;

	if(http_parser_parse_url(url, strlen(url), 0, &pu))
		return NULL;

	req = NULL;
	req_line = NULL;
	ai = NULL;
	sd = -1;

	hostname = w_malloc(pu.field_data[UF_HOST].len + 1);
	memcpy(hostname, url + pu.field_data[UF_HOST].off, pu.field_data[UF_HOST].len);
	hostname[pu.field_data[UF_HOST].len] = 0;

	rc = getaddrinfo(hostname, "http", NULL, &ai_result);

	free(hostname);

	if(rc)
		return NULL;

	sd = socket (AF_INET,SOCK_STREAM,0);

	if(sd == -1)
		return NULL;

	for(ai = ai_result; ai != NULL; ai = ai->ai_next) {
		if(0 == connect(sd, ai->ai_addr, ai->ai_addrlen))
			break;
	}

	if(ai == NULL)
		goto fail;

	req_line = create_request_line(url, &pu, "GET");

	fprintf(stderr, "req: %s", req_line);

	r = write(sd, req_line, strlen(req_line));

	free((void*)req_line);
	req_line = NULL;

	if(r == -1) 
		goto fail;

	if(-1 == fcntl(sd, F_SETFL, O_NONBLOCK)) 
		goto fail;

	req = w_calloc(1, sizeof(*req));
	req->fd = sd;
	req->loop = ev_loop_new(0);
	req->w.data = req;
	ev_io_init(&req->w, read_cb, sd, EV_READ);

	req->obuf = obuf_alloc(8192);

	http_parser_init(&req->parser, HTTP_RESPONSE);
	req->parser.data = req;

	req->settings.on_url = on_url_cb;
	req->settings.on_body = on_body_cb;
	req->settings.on_message_begin = on_message_begin_cb;
	req->settings.on_message_complete = on_message_complete_cb;
	//req->settings.on_header_field = on_h_field_cb;
	//req->settings.on_header_value = on_h_value_cb;

	ev_io_start(req->loop, &req->w);

	ev_loop(req->loop, 0);

	return req;

fail:
	if(req_line)
		free((void*)req_line);
	if(req) {
		if(req->obuf)
			obuf_free(req->obuf);
		if(req->loop)
			ev_loop_destroy(req->loop);
		free(req);
	}
	if(sd != -1)
		close(sd);
	return NULL;
}

void httpc_free_request(struct http_request *req) {
	if(req) {
		if(req->obuf)
			obuf_free(req->obuf);
		if(req->loop)
			ev_loop_destroy(req->loop);
		if(req->fd != -1)
			close(req->fd);
		free(req);
	}
}
