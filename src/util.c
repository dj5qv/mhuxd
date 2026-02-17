/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1


void *w_calloc(size_t nmemb, size_t size) {
	void *p;
	do {
		p = calloc(nmemb, size);
		if(!p)
			sleep(1);
	} while(!p);
	return p;
}

void *w_malloc(size_t size) {
	void *p;
	do {
		p = malloc(size);
		if(!p)
			sleep(1);
	} while(!p);
	return p;
}

char *w_strdup(const char *src) {
	return strdup(src);

	char *n = w_malloc(strlen(src) + 1);
	strcpy(n, src);
	return n;
}

void *w_realloc (void *ptr, size_t size) {
	void *p;

	if(!size) {
		free(ptr);
		return NULL;
	}

	do {
		p = realloc(ptr, size);
		if(p == NULL)
			sleep(1);
	} while(!p);

	return p;
}

int fd_close(int *fd) {
	int cfd;

	if(!fd || *fd < 0)
		return 0;

	cfd = *fd;
	*fd = -1;

	if(close(cfd) == -1)
		return -errno;

	return 1;
}

const char *io_state_to_str(enum mhuxd_io_state state) {
	switch(state) {
	case MHUXD_IO_OPEN:
		return "OPEN";
	case MHUXD_IO_DRAINING:
		return "DRAINING";
	case MHUXD_IO_FAILED:
		return "FAILED";
	case MHUXD_IO_CLOSED:
		return "CLOSED";
	default:
		return "<INVALID>";
	}
}

int io_state_can_transition(enum mhuxd_io_state from, enum mhuxd_io_state to) {
	if(from == to)
		return 1;

	switch(from) {
	case MHUXD_IO_OPEN:
		return (to == MHUXD_IO_DRAINING || to == MHUXD_IO_FAILED || to == MHUXD_IO_CLOSED);
	case MHUXD_IO_DRAINING:
		return (to == MHUXD_IO_FAILED || to == MHUXD_IO_CLOSED);
	case MHUXD_IO_FAILED:
		return (to == MHUXD_IO_CLOSED);
	case MHUXD_IO_CLOSED:
	default:
		return 0;
	}
}

int io_state_transition(enum mhuxd_io_state *state, enum mhuxd_io_state to) {
	if(!state)
		return 0;

	if(*state == to)
		return 1;

	if(!io_state_can_transition(*state, to))
		return 0;

	*state = to;
	return 1;
}

enum mhuxd_io_rw_result io_read_nonblock(int fd, void *buf, size_t len, ssize_t *io_size, int *errsv) {
	ssize_t r = read(fd, buf, len);

	if(io_size)
		*io_size = r;

	if(r > 0)
		return MHUXD_IO_RW_PROGRESS;

	if(r == 0)
		return MHUXD_IO_RW_EOF;

	if(errno == EAGAIN || errno == EWOULDBLOCK)
		return MHUXD_IO_RW_WOULD_BLOCK;

	if(errsv)
		*errsv = errno;

	return MHUXD_IO_RW_ERROR;
}

enum mhuxd_io_rw_result io_write_nonblock(int fd, const void *buf, size_t len, ssize_t *io_size, int *errsv) {
	ssize_t r = write(fd, buf, len);

	if(io_size)
		*io_size = r;

	if(r > 0)
		return MHUXD_IO_RW_PROGRESS;

	if(r == 0)
		return MHUXD_IO_RW_EOF;

	if(errno == EAGAIN || errno == EWOULDBLOCK)
		return MHUXD_IO_RW_WOULD_BLOCK;

	if(errsv)
		*errsv = errno;

	return MHUXD_IO_RW_ERROR;
}



