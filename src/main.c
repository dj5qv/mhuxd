/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "global.h"
#include "opts.h"
#include "launcher.h"
#include "logger.h"
#include "util.h"

int main(int argc, char **argv) {
	int err;
	struct launcher *lc;

	err = opt_parse(argc, argv);

	if(err) {
		fprintf(stderr, "Error parsing options!");
		return -1;
	}

	if(!mhux_params.as_daemon) 
		 printf("\n\n%s (C) 2012 Matthias Moeller, DJ5QV\n\n", PACKAGE_STRING);

	if(mhux_params.help)
		return 0;
	
	log_init();
	log_set_level_by_str(mhux_params.log_level);

	if(mhux_params.as_daemon) {
		log_set_ident("mhuxd");
		daemonize();
	}

	lc = launch_start_all();

	launch_stop_all(lc);

	opt_free();

        return 0;
}

