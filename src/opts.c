/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "opts.h"
#include "util.h"
#include "config.h"

#define DEF_WEBUI_HOST_PORT "localhost:5052"

int background = 0;
int demo_mode = 0;
const char *log_level_str = "INFO";
const char *webui_host_port = DEF_WEBUI_HOST_PORT;

static void print_help() {
	puts("\nUsage: " PACKAGE_NAME " [options]\n");
	puts("    -h                  print this help and exit");
	puts("    -w <interface:port> interface and port of the webserver");
        puts("                        (default: " DEF_WEBUI_HOST_PORT);
	puts("    -b                  run in background, log to logfile");
	puts("    -l <log level>      set the initial log level");
	puts("                        (MUTE|CRIT|ERROR|WARN|INFO|DEBUG0|DEBUG1)");
	puts("\n");
}


int process_opts(int argc, char **argv) {
	int opt;

	while(-1 != (opt = getopt(argc, argv, "2hbw:l:"))) {
		switch(opt) {
		case 'h':
			print_help();
			exit(0);
		case 'w':
			webui_host_port = optarg;
			break;
		case 'b':
			background = 1;
			break;
		case 'l':
			log_level_str = optarg;
			break;
		case '2':
			demo_mode = 1;
			break;
		default:
			exit(-1);
			break;

		}
	}
	return 0;
}
