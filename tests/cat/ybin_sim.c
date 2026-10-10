/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * An emulated older Yaesu radio (ybin_radio.c) on a pseudo terminal, or on a
 * serial port, e.g. one wired to the keyer's radio port:
 *
 *   ybin_sim -m ft1000mp [-d /dev/ttyUSB1 -b 4800] [-v]
 *
 * Without -d it prints the pseudo terminal to give to an application, e.g.
 * rigctl -m 1024 -r /dev/pts/5 f m. -v prints commands and answers.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "ybin_radio.h"

#define ANSWER_DELAY_US 10000	// the radio's processing time

static double now(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static speed_t baud_of(int b) {
	switch(b) {
	case 1200: return B1200;
	case 2400: return B2400;
	case 4800: return B4800;
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	default: return 0;
	}
}

static int open_port(const char *dev, int baud) {
	struct termios tio;
	int fd;

	if(!dev) {
		fd = posix_openpt(O_RDWR | O_NOCTTY);
		if(fd < 0 || grantpt(fd) || unlockpt(fd)) {
			perror("posix_openpt");
			return -1;
		}
		printf("%s\n", ptsname(fd));
		fflush(stdout);
	} else {
		fd = open(dev, O_RDWR | O_NOCTTY);
		if(fd < 0) {
			perror(dev);
			return -1;
		}
	}
	if(tcgetattr(fd, &tio) == 0) {
		cfmakeraw(&tio);
		tio.c_cflag |= CSTOPB | CLOCAL | CREAD;		// 8N2
		if(dev && baud_of(baud)) {
			cfsetispeed(&tio, baud_of(baud));
			cfsetospeed(&tio, baud_of(baud));
		}
		tcsetattr(fd, TCSANOW, &tio);
	}
	return fd;
}

static void hex(const char *tag, const uint8_t *b, size_t n) {
	printf("%10.3f %s", now(), tag);
	for(size_t i = 0; i < n; i++)
		printf(" %02x", b[i]);
	printf("\n");
	fflush(stdout);
}

int main(int argc, char **argv) {
	const char *model = "ft817", *dev = NULL;
	int baud = 4800, verbose = 0, opt, fd;
	static uint8_t out[2048];
	uint8_t cmd[5];
	size_t clen = 0;

	while((opt = getopt(argc, argv, "m:d:b:v")) != -1) {
		switch(opt) {
		case 'm': model = optarg; break;
		case 'd': dev = optarg; break;
		case 'b': baud = atoi(optarg); break;
		case 'v': verbose = 1; break;
		default:
			fprintf(stderr, "usage: %s -m model [-d device -b baud] [-v]\n", argv[0]);
			return 2;
		}
	}
	struct ybin_radio *r = ybr_create(model);
	if(!r) {
		fprintf(stderr, "unknown model %s\n", model);
		return 2;
	}
	if((fd = open_port(dev, baud)) < 0)
		return 1;

	for(;;) {
		struct pollfd p = { .fd = fd, .events = POLLIN };
		uint8_t buf[64];
		ssize_t n;

		if(poll(&p, 1, -1) < 0 && errno != EINTR)
			break;
		n = read(fd, buf, sizeof(buf));
		if(n < 0 && errno == EIO) {
			usleep(100000);		// pseudo terminal not opened yet, or closed
			continue;
		}
		if(n <= 0)
			break;
		for(ssize_t i = 0; i < n; i++) {
			size_t len = ybr_rx(r, buf[i], now(), out);
			cmd[clen++ % 5] = buf[i];
			if(clen % 5 == 0 && verbose)
				hex("<", cmd, 5);
			if(!len)
				continue;
			usleep(ANSWER_DELAY_US);
			unsigned pacing = ybr_state(r)->pacing_ms;
			for(size_t j = 0; j < len; j++) {
				if(write(fd, &out[j], 1) != 1)
					break;
				if(pacing)
					usleep(pacing * 1000);
			}
			if(verbose)
				hex(">", out, len > 32 ? 32 : len);
		}
	}
	ybr_destroy(r);
	return 0;
}
