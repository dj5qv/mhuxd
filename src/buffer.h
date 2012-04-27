/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef BUFFER_H
#define BUFFER_H

#include <assert.h>
#include <string.h>

#define BUFFER_CAPACITY 4096


struct buffer {
	unsigned char data[BUFFER_CAPACITY];
	unsigned size;
	unsigned rpos;
};

struct buffer *buf_alloc();
void buf_free(struct buffer *b);

inline static void buf_reset(struct buffer *b) {
	b->size = 0;
	b->rpos = 0;
}

inline static unsigned buf_size_avail(struct buffer *b) {
	return BUFFER_CAPACITY - b->size;
}

inline static void buf_inc_size(struct buffer *b, int size_inc) {
	b->size += size_inc;
	assert(b->size <= BUFFER_CAPACITY);
}

inline static void buf_inc_rpos(struct buffer *b, int size_inc) {
	assert(b->rpos + size_inc <= BUFFER_CAPACITY);
	b->rpos += size_inc;
        if(b->rpos == b->size)
                buf_reset(b);
}

inline static int buf_append_c(struct buffer *b, unsigned char c) {
	if(b->size == BUFFER_CAPACITY)
		return -1;
	b->data[b->size++] = c;
	return 0;
}

inline static int buf_append(struct buffer *b, const unsigned char *p, int len) {
	int avail = buf_size_avail(b);
	if(len > avail)
		len = avail;

	memcpy(b->data + b->size, p, len);
        b->size += len;
	return len;
}

inline static int buf_get_c(struct buffer *b) {
        int c;
	if(!b || b->rpos == b->size)
		return -1;
        c = b->data[b->rpos++];
        if(b->rpos == b->size)
                buf_reset(b);
        return c;
}


#endif // BUFFER_H
