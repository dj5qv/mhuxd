#ifndef OBUF_H
#define OBUF_H 1


#include <stdio.h>
#include "util.h"

struct obuf {
	size_t capacity;
	size_t size;
	char *data;
};

static struct obuf *obuf_alloc(size_t size) {
	struct obuf *obuf = w_malloc(sizeof(*obuf));
	obuf->data = w_malloc(size);
	obuf->capacity = size;
	obuf->size = 0;
	return obuf;
}

static struct obuf *obuf_realloc(struct obuf *obuf, size_t new_size) {
	obuf->data = w_realloc(obuf->data, new_size);
	obuf->capacity = new_size;

	dbg1("realloc ptr %p new size %zd", obuf->data, new_size);

	return obuf;
}

#include "logger.h"

static void obuf_free(struct obuf * obuf) {
	if(!obuf)
		return;
	if(obuf->data) {
		free(obuf->data);
	}
	free(obuf);
}

static size_t obuf_avail(struct obuf *obuf) {
	return obuf->capacity - obuf->size;
}

#endif /* OBUF_H */
