/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
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

void buf_reset(struct buffer *b) {
	b->size = 0;
	b->rpos = 0;
}

uint16_t buf_size_avail(struct buffer *b) {
	return BUFFER_CAPACITY - b->size;
}

void buf_add_size(struct buffer *b, uint16_t size_inc) {
	b->size += size_inc;
}

void buf_consume(struct buffer *b, uint16_t size_inc) {
	b->rpos += size_inc;
        if(b->rpos == b->size)
                buf_reset(b);
}

int buf_append_c(struct buffer *b, unsigned char c) {
	if(b->size == BUFFER_CAPACITY)
		return -1;
	b->data[b->size++] = c;
	return 0;
}

int buf_append(struct buffer *b, const unsigned char *p, uint16_t len) {
	uint16_t avail = buf_size_avail(b);
	if(len > avail)
		len = avail;

	memcpy(b->data + b->size, p, len);
        b->size += len;
	return len;
}

int buf_append_str(struct buffer *b, const unsigned char *p) {
	return buf_append(b, p, strlen((char*)p));
}

int buf_get_c(struct buffer *b) {
        int c;
	if(!b || b->rpos == b->size)
		return -1;
        c = b->data[b->rpos++];
        if(b->rpos == b->size)
                buf_reset(b);
        return c;
}

void buf_remove_front(struct buffer *b, uint16_t len) {
	memmove(b->data, b->data + len, b->size - len);
	b->size -= len;
	if(b->rpos <= len)
		b->rpos = 0;
	else
		b->rpos -= len;
}
