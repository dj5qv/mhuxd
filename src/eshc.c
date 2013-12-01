/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ev.h>
#include "util.h"
#include "logger.h"
#include "http_client.h"

struct eshc {
	struct ev_loop *loop;
	pthread_t t_gcv;
	ev_timer gcv_start_timer;
	ev_timer gcv_cancel_timer;
	uint8_t gcv_pending;
};

struct eshc eshc;

static void *t_get_current_version(void *ptr) {
	(void)ptr;
	struct http_request *req;

	req = httpc_get_request("http://download.dj5qv.de/mhuxd/version");

	return NULL;
}

static void get_current_version() {
	if(eshc.gcv_pending)
		return;

	fprintf(stderr, "req\n");

	pthread_create(&eshc.t_gcv, NULL, t_get_current_version, NULL);
	eshc.gcv_pending = 1;
	ev_timer_start(eshc.loop, &eshc.gcv_cancel_timer);
}

static void cancel_get_current_version() {
	fprintf(stderr," cancel try\n");
	if(!eshc.gcv_pending)
		return;

	if(pthread_tryjoin_np(eshc.t_gcv, NULL))
		return;

	eshc.gcv_pending = 0;
	ev_timer_stop(eshc.loop, &eshc.gcv_cancel_timer);
	ev_timer_start(eshc.loop, &eshc.gcv_start_timer);

	fprintf(stderr," cancel ok\n");
}

void eshc_init(struct ev_loop *loop) {
	memset(&eshc, 0, sizeof(eshc));
	eshc.loop = loop;
	ev_timer_init(&eshc.gcv_start_timer, get_current_version, 2, 10);
	ev_timer_init(&eshc.gcv_cancel_timer, cancel_get_current_version, 0, 1);
	ev_timer_start(loop, &eshc.gcv_start_timer);
}

void eshc_cleanup() {
	if(eshc.gcv_pending) {
		pthread_cancel(eshc.t_gcv);
		pthread_join(eshc.t_gcv, NULL);
		eshc.gcv_pending = 0;
		ev_timer_stop(eshc.loop, &eshc.gcv_cancel_timer);
	} else {
		ev_timer_stop(eshc.loop, &eshc.gcv_start_timer);
	}
}
