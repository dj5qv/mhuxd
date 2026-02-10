/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <string.h>

#define BUFFER_CAPACITY 512


struct buffer {
	unsigned char data[BUFFER_CAPACITY];
	uint16_t size;
	uint16_t rpos;
};

struct buffer *buf_alloc();
void buf_free(struct buffer *b);
void buf_reset(struct buffer *b);
uint16_t buf_size_avail(struct buffer *b);
void buf_add_size(struct buffer *b, uint16_t size_inc);
void buf_consume(struct buffer *b, uint16_t size_inc);
int buf_append_c(struct buffer *b, unsigned char c);
int buf_append(struct buffer *b, const unsigned char *p, uint16_t len);
int buf_append_str(struct buffer *b, const unsigned char *p);
int buf_get_c(struct buffer *b);
void buf_remove_front(struct buffer *b, uint16_t len);



#endif // BUFFER_H

