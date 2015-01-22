/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdint.h>
#include <string.h>
#include "util.h"
#include "dbuf.h"


#define DEFAULT_SIZE_INC (1024)
#define DEFAULT_SIZE_INITIAL (1024)


struct dbuf *dbuf_create() {
	struct dbuf *dbuf = w_calloc(1, sizeof(*dbuf));
	dbuf->size_initial = DEFAULT_SIZE_INITIAL;
	dbuf->size_inc = DEFAULT_SIZE_INC;
	return dbuf;
}

void dbuf_destroy(struct dbuf *dbuf) {
	free(dbuf);
}

void dbuf_set_initial_size(struct dbuf *dbuf, size_t size) {
	dbuf->size_initial = size;
}

void dbuf_set_inc_size(struct dbuf *dbuf, size_t size) {
	dbuf->size_inc = size;
}

void dbuf_append(struct dbuf *dbuf, uint8_t *data, size_t length) {
	size_t avail;


	avail = dbuf->capacity - dbuf->size;

	if(avail < length) {
		size_t need = length - avail;
		dbuf->capacity = need > dbuf->size_inc ? need : dbuf->size_inc;
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
