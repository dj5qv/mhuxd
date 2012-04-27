/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHMX_H
#define MHMX_H

#include "mhproto.h"
#include "buffer.h"

struct mx {
	struct buffer *ibuf[MH_CHANNEL_MAX];
};


struct mx *mx_create();
void mx_destroy(struct mx *mx);
int mx_mux(struct mx *mx, struct buffer *bout);




#endif // MHMX_H
