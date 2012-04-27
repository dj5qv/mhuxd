/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef OPTS_H
#define OPTS_H

struct mhux_param {
#if 0
	const char *dev;
	const char *serial;
#endif
	const char *cfg_file;
	const char *log_level;
        int     help;
	int	as_daemon;
};

extern struct mhux_param mhux_params;

int opt_parse(int argc, char **argv);
void opt_free();

#endif // OPTS_H
