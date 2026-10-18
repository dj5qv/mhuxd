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
 *   ybin_sim -m ft1000mp [-d /dev/ttyUSB1 -b 4800] [-s 14074000:usb,7025000:cw] [-v]
 *
 * Without -d it prints the pseudo terminal to give to an application, e.g.
 * rigctl -m 1024 -r /dev/pts/5 f m. -v prints commands and answers.
 * -s: states of VFO A (Hz:mode); the first one at start, SIGUSR1 steps to the
 * next (kill -USR1), e.g. to check what an application makes of each mode.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "ybin_radio.h"

#define ANSWER_DELAY_US 10000	// the radio's processing time
#define MAX_STATES 16

struct sim_state {
	uint32_t hz;
	uint8_t mode;
};

static const char *const mode_names[] = {
	[YBR_LSB] = "lsb", [YBR_USB] = "usb", [YBR_CW] = "cw", [YBR_CWR] = "cwr", [YBR_AM] = "am",
	[YBR_AMS] = "ams", [YBR_FM] = "fm", [YBR_WFM] = "wfm", [YBR_RTTY] = "rtty",
	[YBR_RTTYR] = "rttyr", [YBR_PKTL] = "pktl", [YBR_PKTU] = "pktu", [YBR_PKTFM] = "pktfm",
	[YBR_DIG] = "dig",
};

static volatile sig_atomic_t next_state;

static void on_usr1(int sig) {
	(void)sig;
	next_state = 1;
}

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

// "14074000:usb,7025000:cw" -> st[], returns the number of states, 0 if malformed.
static int parse_states(char *arg, struct sim_state *st) {
	int n = 0;

	for(char *tok = strtok(arg, ","); tok; tok = strtok(NULL, ",")) {
		char *colon = strchr(tok, ':');
		size_t m;

		if(!colon || n == MAX_STATES)
			return 0;
		*colon = 0;
		for(m = 0; m < sizeof(mode_names) / sizeof(mode_names[0]); m++)
			if(!strcmp(colon + 1, mode_names[m]))
				break;
		if(m == sizeof(mode_names) / sizeof(mode_names[0]))
			return 0;
		st[n].hz = strtoul(tok, NULL, 10);
		st[n++].mode = m;
	}
	return n;
}

static void set_state(struct ybin_radio *r, const struct sim_state *st) {
	ybr_state(r)->freq[0] = st->hz;
	ybr_state(r)->mode[0] = st->mode;
	printf("%10.3f state VFO A %u %s\n", now(), st->hz, mode_names[st->mode]);
	fflush(stdout);
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
	int baud = 4800, verbose = 0, opt, fd, nstates = 0, cur = 0;
	struct sim_state states[MAX_STATES];
	static uint8_t out[2048];
	uint8_t cmd[5];
	size_t clen = 0;

	while((opt = getopt(argc, argv, "m:d:b:s:v")) != -1) {
		switch(opt) {
		case 'm': model = optarg; break;
		case 'd': dev = optarg; break;
		case 'b': baud = atoi(optarg); break;
		case 's':
			if(!(nstates = parse_states(optarg, states))) {
				fprintf(stderr, "bad state list, e.g. 14074000:usb,7025000:cw\n");
				return 2;
			}
			break;
		case 'v': verbose = 1; break;
		default:
			fprintf(stderr, "usage: %s -m model [-d device -b baud] [-s hz:mode,...] [-v]\n",
				argv[0]);
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
	if(nstates) {
		struct sigaction sa = { .sa_handler = on_usr1 };	// no SA_RESTART: wakes poll()
		sigaction(SIGUSR1, &sa, NULL);
		set_state(r, &states[0]);
	}

	for(;;) {
		struct pollfd p = { .fd = fd, .events = POLLIN };
		uint8_t buf[64];
		ssize_t n;

		if(next_state) {
			next_state = 0;
			cur = (cur + 1) % nstates;
			set_state(r, &states[cur]);
		}
		if(poll(&p, 1, -1) < 0) {
			if(errno == EINTR)
				continue;
			break;
		}
		n = read(fd, buf, sizeof(buf));
		if(n < 0 && errno == EINTR)
			continue;
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
