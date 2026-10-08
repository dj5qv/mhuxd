/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <sys/types.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#define STR(tok) #tok
#define STRINGIFY(tok) STR(tok)

void *w_calloc(size_t nmemb, size_t size);
void *w_malloc(size_t size);
char *w_strdup(const char *src);
void *w_realloc (void *ptr, size_t size);
int fd_close(int *fd);

enum mhuxd_io_state {
	MHUXD_IO_OPEN = 0,
	MHUXD_IO_DRAINING,
	MHUXD_IO_FAILED,
	MHUXD_IO_CLOSED,
};

enum mhuxd_io_rw_result {
	MHUXD_IO_RW_PROGRESS = 0,
	MHUXD_IO_RW_WOULD_BLOCK,
	MHUXD_IO_RW_EOF,
	MHUXD_IO_RW_ERROR,
};

const char *io_state_to_str(enum mhuxd_io_state state);
int io_state_can_transition(enum mhuxd_io_state from, enum mhuxd_io_state to);
int io_state_transition(enum mhuxd_io_state *state, enum mhuxd_io_state to);
enum mhuxd_io_rw_result io_read_nonblock(int fd, void *buf, size_t len, ssize_t *io_size, int *errsv);
enum mhuxd_io_rw_result io_write_nonblock(int fd, const void *buf, size_t len, ssize_t *io_size, int *errsv);

#endif // UTIL_H
