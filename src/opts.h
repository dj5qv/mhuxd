/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef OPTS_H
#define OPTS_H

extern int background;
extern int demo_mode;
extern const char *log_level_str;
extern const char *webui_host_port;

int process_opts(int argc, char **argv);


#endif /* OPTS_H */
