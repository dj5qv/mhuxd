/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <string.h>

#define BUFFER_CAPACITY 64


struct buffer {
	unsigned char data[BUFFER_CAPACITY];
	uint16_t size;
	uint16_t rpos;
};

struct buffer *buf_alloc();
void buf_free(struct buffer *b);

inline static void buf_reset(struct buffer *b) {
	b->size = 0;
	b->rpos = 0;
}

inline static uint16_t buf_size_avail(struct buffer *b) {
	return BUFFER_CAPACITY - b->size;
}

inline static void buf_add_size(struct buffer *b, uint16_t size_inc) {
	b->size += size_inc;
}

inline static void buf_consume(struct buffer *b, uint16_t size_inc) {
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

inline static int buf_append(struct buffer *b, const unsigned char *p, ssize_t len) {
	uint16_t avail = buf_size_avail(b);
	if(len > avail)
		len = avail;

	memcpy(b->data + b->size, p, len);
        b->size += len;
	return len;
}

inline static int buf_append_str(struct buffer *b, const unsigned char *p) {
	return buf_append(b, p, strlen((char*)p));
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

inline static void buf_remove_front(struct buffer *b, uint16_t len) {
	memmove(b->data, b->data + len, b->size - len);
	b->size -= len;
	if(b->rpos <= len)
		b->rpos = 0;
	else
		b->rpos -= len;
}


#endif // BUFFER_H

