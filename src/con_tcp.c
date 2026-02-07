/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2017  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <ev.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "con_tcp.h"
#include "logger.h"
#include "util.h"
#include "buffer.h"
#include "pglist.h"
#include "conmgr.h"
#include "net.h"

#define MOD_ID "tcp"

struct ctcp {
	int fd_data;
	struct netlsnr *lsnr;
	ev_io w_lsnr;;
	ev_io w_data_in;
	ev_io w_data_out;

	int max_con;
	int open_cnt;
	struct PGList session_list;

	struct ev_loop *loop;
	char *devname;
};

struct ctcp_session {
	struct PGNode node;
	struct ctcp *ctcp;
	int fd;
	ev_io w_in;
	ev_io w_out;
	struct buffer buf_out;
	struct buffer buf_in;
};

static void rem_session(struct ctcp_session *cs) {
	struct ctcp *ctcp = cs->ctcp;

	ev_io_stop(ctcp->loop, &cs->w_in);
	ev_io_stop(ctcp->loop, &cs->w_out);

	close(cs->fd);
	ctcp->open_cnt--;
	PG_Remove(&cs->node);
	free(cs);
}

static void data_in_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)revents;
	struct ctcp *ctcp = w->data;
	int r;
	uint8_t buf[1024];

	r = read(w->fd, buf, sizeof(buf));
	if(r <= 0) {
		err_e(errno, "error reading from data socket!");
		ev_io_stop(loop, &ctcp->w_data_in);
		// FIXME: better error handling needed.
	}

	if(r == 0) {
		return;
	}

	struct ctcp_session *cs;
	PG_SCANLIST(&ctcp->session_list, cs) {
		if(r != buf_append(&cs->buf_out, buf, r))
				warn("buffer overflow writing to client!");
		ev_io_start(loop, &cs->w_out);
	}
}

static void data_out_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)revents;
	struct ctcp *ctcp = w->data;
	struct ctcp_session *cs;
	int size;
	int need_to_write = 0;

	PG_SCANLIST(&ctcp->session_list, cs) {
		struct buffer *b = &cs->buf_in;
		if(b->rpos == b->size)
			continue;
		size = write(ctcp->fd_data, b->data + b->rpos, b->size - b->rpos);
		if(size < 0 || (size == 0 && errno != EAGAIN)) {
			err_e(errno, "error reading from data socket!");
			ev_io_stop(loop, &ctcp->w_data_out);
			// FIXME: better error handling needed.
		}

		buf_consume(b, size);
		if(b->rpos < b->size)
			need_to_write = 1;

	}

	if(!need_to_write)
		ev_io_stop(loop, &ctcp->w_data_out);
}

static void client_in_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)revents;
	struct ctcp_session *cs = w->data;
	struct ctcp *ctcp = cs->ctcp;
	int avail;
	int r;

	avail = buf_size_avail(&cs->buf_in);
	r = read(cs->fd, cs->buf_in.data + cs->buf_in.size, avail);
	if(r <= 0) {
		info("connection on %s closed", ctcp->devname);
		rem_session(cs);
		return;
	}

	buf_add_size(&cs->buf_in, r);
	ev_io_start(loop, &ctcp->w_data_out);
}

static void client_out_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)revents;
	struct ctcp_session *cs = w->data;
	struct ctcp *ctcp = cs->ctcp;
	int r;

	r = write(cs->fd, cs->buf_out.data + cs->buf_out.rpos, cs->buf_out.size - cs->buf_out.rpos);
	if(r < 0 || (r == 0 && errno != EAGAIN)) {
		info("connection on %s closed", ctcp->devname);
		rem_session(cs);
		return;
	}

	buf_consume(&cs->buf_out, r);

	if(!cs->buf_out.size)
		ev_io_stop(loop, &cs->w_out);
}

static void lsnr_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct ctcp *ctcp = w->data;
	int fd;

	fd = net_accept(w->fd);
	if(fd == -1) {
		err_e(errno, "accept() failed on tcp connector %s!", ctcp->devname);
		return;
	}

	if(ctcp->open_cnt >= ctcp->max_con) {
		info("rejecting connect, maximum connections reached for %s", ctcp->devname);
		close(fd);
		return;
	}

	struct ctcp_session *cs = w_calloc(1, sizeof(*cs));
	cs->ctcp = ctcp;
	cs->fd = fd;
	ev_io_init(&cs->w_in, client_in_cb, fd, EV_READ);
	ev_io_init(&cs->w_out, client_out_cb, fd, EV_WRITE);
	ev_io_start(ctcp->loop, &cs->w_in);
	cs->w_in.data = cs;
	cs->w_out.data = cs;

	ctcp->open_cnt++;

	PG_AddTail(&ctcp->session_list, &cs->node);

	info("incoming connection on %s", ctcp->devname);
}


struct ctcp *ctcp_create(const struct connector_spec *cpsec) {
	struct ctcp *ctcp;

	dbg1("%s()", __func__);

	const char *port = cpsec->tcp.port;
	if(port == NULL || !isdigit(*port)) {
		err("could not create tcp connector: no or invalid port specified!");
		return NULL;
	}

	int8_t remote_access = (int8_t)(cpsec->tcp.remote_access ? 1 : 0);

	char devname[128];
	if(128 <= snprintf(devname, 128, "%s:%s", remote_access ? "0.0.0.0" : "127.0.0.1", port)) {
		err("could not create tcp connector: no or invalid parameter!");
		return NULL;
	}

	struct netlsnr *lsnr = net_create_listener(devname);
	if(lsnr == NULL) {
		err_e(errno, "could not create listener %s!", devname);
		return NULL;
	}

	ctcp = w_calloc(1, sizeof(*ctcp));
	PG_NewList(&ctcp->session_list);
	ctcp->loop = cpsec->loop;
	ctcp->devname = w_strdup(devname);
	ctcp->lsnr = lsnr;
	ctcp->fd_data = cpsec->fd_data;
	ctcp->max_con = (cpsec->tcp.maxcon > 0) ? cpsec->tcp.maxcon : 1;

	ev_io_init(&ctcp->w_data_in, data_in_cb, ctcp->fd_data, EV_READ);
	ev_io_init(&ctcp->w_data_out, data_out_cb, ctcp->fd_data, EV_WRITE);
	ev_io_init(&ctcp->w_lsnr, lsnr_cb, net_listener_get_fd(lsnr), EV_READ);
	ctcp->w_data_in.data = ctcp;
	ctcp->w_data_out.data = ctcp;
	ctcp->w_lsnr.data = ctcp;

	ev_io_start(ctcp->loop, &ctcp->w_lsnr);
	ev_io_start(ctcp->loop, &ctcp->w_data_in);
	info("tcp connector %s created", ctcp->devname);

	return ctcp;
}

void ctcp_destroy(struct ctcp *ctcp) {
	struct ctcp_session *cs;

	ev_io_stop(ctcp->loop, &ctcp->w_data_in);
	ev_io_stop(ctcp->loop, &ctcp->w_data_out);
	ev_io_stop(ctcp->loop, &ctcp->w_lsnr);

	while((cs = (void*)PG_FIRSTENTRY(&ctcp->session_list)))
			rem_session(cs);

	close(ctcp->fd_data);
	net_destroy_lsnr(ctcp->lsnr);

	info("tcp connector %s closed", ctcp->devname);

	if(ctcp->devname)
		free(ctcp->devname);
	free(ctcp);
}
