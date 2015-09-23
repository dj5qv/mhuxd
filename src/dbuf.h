/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef DBUF_H
#define DBUF_H

struct dbuf {
	size_t capacity;
	size_t size;
	size_t size_inc;
	uint8_t *data;
};

struct dbuf *dbuf_create();
void dbuf_destroy(struct dbuf *dbuf) ;
void dbuf_set_inc_size(struct dbuf *dbuf, size_t size);
void dbuf_append(struct dbuf *dbuf, uint8_t *data, size_t length);
void dbuf_append_c(struct dbuf *dbuf, int c);
void dbuf_reset(struct dbuf *dbuf);


#endif /* DBUF_H */
