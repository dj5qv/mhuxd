/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CTLMSG_H
#define CTLMSG_H

#include <stdint.h>

enum {
	CMID_INVALID = 0,
	CMID_PTT     = 1,
};

struct ctlmsg {
	uint16_t id;
	uint16_t reserved;
	uint32_t data;
};

#define CTLMSG_SIZE sizeof(struct ctlmsg)

int send_ctlmsg(int fd, int id, int data);
int recv_ctlmsg(int fd, struct ctlmsg *msg);

#endif // CTLMSG_H
