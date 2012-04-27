/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
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

int tty_open(const char *name) {
	int fd;

	if(!name)
		return -1;

	fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if(fd == -1)
		return -1;

	ioctl(fd, TIOCEXCL, NULL);

	struct termios newtio;
	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = B230400 | CS8 | CLOCAL | CREAD | CRTSCTS;
	//cfmakeraw(&newtio);
	int err = tcsetattr(fd, TCSANOW, &newtio);

	if(err) {
		err_e(-errno, "Could not set attributes on %s!", name);
		close(fd);
		return -1;
	}

	return fd;
}

int tty_close(int fd) {
	return close(fd);
}

