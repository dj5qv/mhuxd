/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef ESHC_H
#define ESHC_H

void eshc_init(struct ev_loop *loop);
void eshc_cleanup();

#endif // ESHC_H
