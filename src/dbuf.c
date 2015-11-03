/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dbuf.h"
#include "util.h"

#define DEFAULT_SIZE_INC (64)

struct dbuf *dbuf_create() {
	struct dbuf *dbuf = w_calloc(1, sizeof(*dbuf));
	dbuf->size_inc = DEFAULT_SIZE_INC;
	return dbuf;
}

void dbuf_destroy(struct dbuf *dbuf) {
	if(dbuf->data)
		free(dbuf->data);
	free(dbuf);
}

void dbuf_set_inc_size(struct dbuf *dbuf, size_t size) {
	if(!size)
		return;
	dbuf->size_inc = size;
}

void dbuf_append(struct dbuf *dbuf, uint8_t *data, size_t length) {
	size_t avail;

	if(!length)
		return;

	avail = dbuf->capacity - dbuf->size;

	if(avail < length) {
		dbuf->capacity += (((length / dbuf->size_inc) + 1) * dbuf->size_inc);
		dbuf->data = w_realloc(dbuf->data, dbuf->capacity);
	}

	memcpy(dbuf->data + dbuf->size, data, length);
	dbuf->size += length;
}

void dbuf_append_c(struct dbuf *dbuf, int c) {
	uint8_t val = c & 0xff;
	dbuf_append(dbuf, &val, 1);
}

void dbuf_reset(struct dbuf *dbuf) {
	memset(dbuf->data, 0, dbuf->size);
	dbuf->size = 0;
}
