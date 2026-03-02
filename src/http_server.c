/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
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
#include <sys/socket.h>
#include <fcntl.h>
#include "config.h"
#include "http_server.h"
#include "http_server_int.h"
#include "ws.h"
#include "http_parser.h"
#include "http_codes.h"
#include "util.h"
#include "pglist.h"
#include "net.h"
#include "logger.h"

#define MOD_ID "http"

#define READ_SIZE (1024)
#define OUTQ_SOFT_LIMIT (256 * 1024)
#define OUTQ_HARD_LIMIT (1024 * 1024)

enum {
	HEADER_STATE_VALUE,
	HEADER_STATE_FIELD
};

static char *rfc1123_date_time(char buf[30], time_t *t);
static char *rfc1123_current_date_time(char buf[30]);

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



static void clear_response_queue(struct http_connection *hcon) {
    struct http_response *hr;
    while((hr = (void*)PG_FIRSTENTRY(&hcon->response_list))) {
        PG_Remove(&hr->node);
        hcon->outq_dropped_bytes += hr->len;
        free(hr);
    }
    hcon->outq_bytes = 0;
    hcon->outq_warned = 0;
}

static void hcon_mark_dropped(struct http_connection *hcon) {
    if(hcon->terminate)
        return;

    hcon->terminate = 1;
    ev_io_stop(hcon->hs->loop, &hcon->w_in);
    clear_response_queue(hcon);

    /* force kernel-level close progression */
    shutdown(hcon->fd, SHUT_RDWR);

    /* let out-cb/removal path finalize */
    ev_io_start(hcon->hs->loop, &hcon->w_out);
	ev_feed_event(hcon->hs->loop, &hcon->w_out, EV_WRITE);
}

int queue_response_data(struct http_connection *hcon, const char *data, size_t len) {
    struct http_response *hr;

    if(hcon->terminate)
        return -1;
    if(!len)
        return 0;

    if(hcon->outq_bytes + len > OUTQ_HARD_LIMIT) {
        hcon->outq_hard_hits++;
        warn("http outbound queue hard limit exceeded (fd=%d queued=%zu add=%zu hits=%u)",
             hcon->fd, hcon->outq_bytes, len, hcon->outq_hard_hits);
		   dbg0("http drop snapshot: fd=%d ws=%u tx_bytes=%zu tx_frames=%zu outq_now=%zu outq_peak=%zu outq_dropped=%zu hard_hits=%u",
			   hcon->fd,
			   hcon->ws_enabled ? 1 : 0,
			   hcon->tx_bytes,
			   hcon->tx_frames,
			   hcon->outq_bytes,
			   hcon->outq_peak,
			   hcon->outq_dropped_bytes,
			   hcon->outq_hard_hits);

		if(hcon->ws_enabled && !hcon->terminate) {
			/* clear_response_queue resets outq_bytes to 0 so the
			 * recursive queue_response_data call from
			 * hs_ws_close -> ws_send_frame fits within limits. */
			clear_response_queue(hcon);
			if(hs_ws_close(hcon, 1013, "backpressure") == 0)
				return -1;
		}

        hcon_mark_dropped(hcon);
        return -1;
    }

    if(!hcon->outq_warned && hcon->outq_bytes + len > OUTQ_SOFT_LIMIT) {
        hcon->outq_warned = 1;
        dbg0("http outbound queue soft limit exceeded (fd=%d queued=%zu add=%zu)",
             hcon->fd, hcon->outq_bytes, len);
    }

    hr = w_calloc(1, sizeof(*hr) + len + 1);
    hr->data = (char*)&hr[1];
    memcpy(hr->data, data, len);
    hr->len = len;
    hcon->outq_bytes += len;
    if(hcon->outq_bytes > hcon->outq_peak)
        hcon->outq_peak = hcon->outq_bytes;

    PG_AddTail(&hcon->response_list, &hr->node);
    ev_io_start(hcon->hs->loop, &hcon->w_out);
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

void rem_connection(struct http_connection *hcon) {
	struct http_response *hr;

	if (hcon->close_cb)
		hcon->close_cb(hcon, hcon->close_data);

	ev_io_stop(hcon->hs->loop, &hcon->w_in);
	ev_io_stop(hcon->hs->loop, &hcon->w_out);
	PG_Remove(&hcon->node);

	while((hr = (void*)PG_FIRSTENTRY(&hcon->response_list))) {
		if(hr->len > hr->sent)
			hcon->outq_dropped_bytes += hr->len - hr->sent;
		PG_Remove(&hr->node);
		free(hr);
	}
	hcon->outq_bytes = 0;

	dbg0("http connection stats: fd=%d ws=%u tx_bytes=%zu tx_frames=%zu outq_peak=%zu outq_dropped=%zu outq_hard_hits=%u term=%u",
		hcon->fd,
		hcon->ws_enabled ? 1 : 0,
		hcon->tx_bytes,
		hcon->tx_frames,
		hcon->outq_peak,
		hcon->outq_dropped_bytes,
		hcon->outq_hard_hits,
		hcon->terminate ? 1 : 0);

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

static void hcon_out_cb(struct ev_loop *loop, ev_io *w, int revents) {
	(void)loop; (void)revents;
    struct http_connection *hcon = (struct http_connection *)w->data;
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

        if((size_t)r <= hcon->outq_bytes)
            hcon->outq_bytes -= (size_t)r;
        else
            hcon->outq_bytes = 0;

		hcon->tx_bytes += (size_t)r;

        hr->sent += r;
        if(hr->sent == hr->len) {
			hcon->tx_frames++;
            PG_Remove(&hr->node);
            free(hr);
        }
    }

    if(hcon->outq_warned && hcon->outq_bytes <= OUTQ_SOFT_LIMIT)
        hcon->outq_warned = 0;


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
		dbg0("%s(): accepted new connection, fd=%d", __func__, fd);

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
	hcon->outq_bytes += hr->len;
	if(hcon->outq_bytes > hcon->outq_peak)
		hcon->outq_peak = hcon->outq_bytes;
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
	hcon->outq_bytes += hr->len;
	if(hcon->outq_bytes > hcon->outq_peak)
		hcon->outq_peak = hcon->outq_bytes;
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

	hs = w_calloc(1, sizeof(*hs));
	PG_NewList(&hs->con_list);
	PG_NewList(&hs->handler_list);
	PG_NewList(&hs->dir_map_list);
	hs->loop = loop;
	hs->lsnr_v4 = lsnr_v4;
	hs->lsnr_v6 = lsnr_v6;
	hs->bind_desc = bind_desc;

	if(hs->lsnr_v4) {
		ev_io_init(&hs->w_lsnr_v4, lsnr_cb, net_listener_get_fd(hs->lsnr_v4), EV_READ);
		hs->w_lsnr_v4.data = hs;
		ev_io_start(hs->loop, &hs->w_lsnr_v4);
	}

	if(hs->lsnr_v6) {
		ev_io_init(&hs->w_lsnr_v6, lsnr_cb, net_listener_get_fd(hs->lsnr_v6), EV_READ);
		hs->w_lsnr_v6.data = hs;
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
