/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include "ctlmsg.h"
#include "logger.h"

/* Expectes a blocking fd.
  */
int send_ctlmsg(int fd, int id, int data) {
	struct ctlmsg msg;
	char *p;
	int r, size;

	if(fd == -1)
		return -1;

	msg.id = htons(id);
	msg.reserved = 0;
	msg.data = htonl(data);

	p = (char *)&msg;
	size = CTLMSG_SIZE;

	while(size) {
		r = write(fd, p, size);
		if(r == 0)
			return -1;
		if(r < 0 && errno != EAGAIN && errno != EINTR) {
			return -1;
		}
		size -= r;
		p += r;
	}
	return 0;
}

/* Expects a blocking fd.
  */
int recv_ctlmsg(int fd, struct ctlmsg *msg) {
	char *p;
	int r, size;

	if(fd == -1)
		return -1;

	msg->id = CMID_INVALID;

	p = (char *)msg;
	size = CTLMSG_SIZE;

	while(size)  {
		r = read(fd, p, size);
		if(r == 0)
			return -1;
		if(r < 0 && errno != EAGAIN && errno != EINTR) {
			return -1;
		}
		size -= r;
		p += r;
	}

	msg->id = ntohs(msg->id);
	msg->reserved = 0;
	msg->data = ntohl(msg->data);
	return 0;
}

