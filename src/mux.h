/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHMX_H
#define MHMX_H

#include "buffer.h"

int mx_mux(struct buffer *bout, struct buffer *ibuf);



#endif // MHMX_H
