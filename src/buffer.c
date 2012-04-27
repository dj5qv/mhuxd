/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdlib.h>
#include "buffer.h"

struct buffer *buf_alloc() {
	struct buffer *b = calloc(1, sizeof(*b));
	return b;
}

void buf_free(struct buffer *b) {
	if(b) {
		free(b);
	}
}
