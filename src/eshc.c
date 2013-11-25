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

static int con(const char *host, int port)  {
	int	sd;
	struct	sockaddr_in server;
	struct  hostent *hp, *gethostbyname();

	sd = socket (AF_INET,SOCK_STREAM,0);

	if(sd == -1)
		return -1;

	server.sin_family = AF_INET;
	fprintf(stderr,"host: ==%s==\n", host);
	hp = gethostbyname(host);
	if(!hp) {
		close(sd);
		return -1;
	}

	bcopy ( hp->h_addr, &(server.sin_addr.s_addr), hp->h_length);
	server.sin_port = htons(port);

	if(-1 == connect(sd, &server, sizeof(server))) {
		close(sd);
		sd = -1;
	}

	return sd;
}

void *t_get_current_version_(void *ptr) {
	(void)ptr;
	static const char req_str[] = "GET /mhuxd/current HTTP/1.1\r\nHost: download.dj5qv.de\r\n\r\n";
	ssize_t r;

	int fd = con("download.dj5qv.de", 80);
	if(fd == -1)
		return NULL;

	r = write(fd, req_str, sizeof(req_str));

	if(r != sizeof(req_str)) {
		close(fd);
		return NULL;
	}

	char *buf = w_malloc(4096 + 1);
	r = read(fd, buf, 4096);
	if(r >= 0) {
		buf[r] = 0;
		fprintf(stderr, "Got %zd bytes\n%s", r, buf);
	} else
		fprintf(stderr, "read error\n");


	free(buf);
	close(fd);
	return NULL;
}

void *t_get_current_version(void *ptr) {
	(void)ptr;
	struct http_request *req;

	req = httpc_get_request("http://download.dj5qv.de/mhuxd/current");

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
	fprintf(stderr," cancel\n");
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
