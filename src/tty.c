/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include "tty.h"
#include "logger.h"
#include "pglist.h"
#include "util.h"

#define MOD_ID "tty"

struct fd_node {
	struct PGNode node;
	int fd;
	struct termios termios;
};

// Globals. If we should ever go for multi-threading, these would need to be protected.
static int is_initialized = 0;
static struct PGList fd_list;

int tty_open(const char *name) {
	int fd;
	struct fd_node *fd_node;

	if(!name)
		return -1;

	if(!is_initialized) {
		PG_NewList(&fd_list);
		is_initialized = 1;
	}

	fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if(fd == -1)
		return -1;

	fd_node = w_calloc(1, sizeof(*fd_node));
	fd_node->fd = fd;
	tcgetattr(fd, &fd_node->termios);

	ioctl(fd, TIOCEXCL, NULL);

	struct termios newtio;
	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = B230400 | CS8 | CLOCAL | CREAD | CRTSCTS;
	//cfmakeraw(&newtio);
	int err = tcsetattr(fd, TCSANOW, &newtio);

	if(err) {
		err_e(errno, "could not set attributes on %s!", name);
		close(fd);
		return -1;
	}

	tcflush(fd, TCIOFLUSH);

	PG_AddTail(&fd_list, &fd_node->node);

	return fd;
}

int tty_close(int fd) {
	struct fd_node *fd_node;
	int restored = 0;

	if(is_initialized) {
		PG_SCANLIST(&fd_list, fd_node) {
			if(fd_node->fd == fd) {
				tcsetattr(fd, TCSANOW, &fd_node->termios);
				PG_Remove(&fd_node->node);
				free(fd_node);
				restored = 1;
				break;
			}
		}
	}

	if(!restored)
		warn("Could not restore serial port attributes!");

	return close(fd);
}

