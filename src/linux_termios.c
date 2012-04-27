/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include "linux_termios.h"

static const int baud_tbl[][2] = {
	{ B0, 0 },
	{ B50, 50 },
	{ B75, 75 },
	{ B110, 110 },
	{ B134, 134 },
	{ B150, 150 },
	{ B200, 200 },
	{ B300, 300 },
	{ B600, 600 },
	{ B1200, 1200 },
	{ B1800, 1800 },
	{ B2400, 2400 },
	{ B4800, 4800 },
	{ B9600, 9600 },
	{ B19200, 19200 },
	{ B38400, 38400 },
	{ B57600, 57600 },
#ifdef NO_MICRO_HAM
	{ B115200, 115200 },
	{ B230400, 230400 },
	{ B460800, 460800 },
	{ B500000, 500000 },
	{ B576000, 576000 },
	{ B921600, 921600 },
	{ B1000000, 1000000 },
	{ B1152000, 1152000 },
	{ B1500000, 1500000 },
	{ B2000000, 2000000 },
	{ B2500000, 2500000 },
	{ B3000000, 3000000 },
	{ B3500000, 3500000 },
	{ B4000000, 4000000 },
#endif /* NO_MICRO_HAM */
};

int termios_baud_rate(struct termios *ts) {
	int baud, cbaud;
	unsigned i;

	baud = -1;
	cbaud = ts->c_cflag & CBAUD;

	for(i = 0; i < sizeof(baud_tbl) / (2 * sizeof(int)); i++) {
		if(baud_tbl[i][0] == cbaud) {
			baud = baud_tbl[i][1];
			break;
		}
	}
	return baud;
}
