/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <errno.h>
#include <ev.h>
#include "clearsilver/util/neo_err.h"
#include "config.h"
#include "util.h"
#include "logger.h"
#include "app_ctx.h"
#include "devmgr.h"
#include "conmgr.h"
#include "cfgmgr.h"
#include "opts.h"
#include "daemon.h"
#include "http_server.h"
#include "webui.h"
#include "restapi.h"
#include "cfgmgrj.h"

#define MOD_ID "main"

#ifndef RUNDIR
#error RUNDIR not defined
#endif

#define PIDFILE RUNDIR "/mhuxd.pid"

int main(int argc, char **argv)
{
	struct ev_loop *loop;
	FILE *pidfile = NULL;

	// options
	process_opts(argc, argv);
	if(-1 == log_set_level_by_str(log_level_str)) // from opt.c
		fprintf(stderr, "Invalid log level: %s", log_level_str);

	log_open(background ? 0 : 1);

	if(background) {
		dmn_daemonize();
	}

	info("%s (C)2012-2026 Matthias Moeller, DJ5QV", PACKAGE_STRING);
	info("Logfile: %s", log_get_file_name());

	// pid file
	pidfile = dmn_pidfile_lock(PIDFILE);
	if(pidfile == NULL)
		return -1;

	// ev setup
	ev_set_allocator((void*)w_realloc);
	loop = ev_default_loop(EVFLAG_AUTO);

	// App context, orchestrator.
	struct app_ctx *app_ctx = app_ctx_create();
	app_ctx_init(app_ctx, loop);

	app_ctx_run(app_ctx);

	app_ctx_destroy(app_ctx);

	ev_default_destroy();

	dmn_pidfile_unlock(pidfile, PIDFILE);

	log_close();

	nerr_free();

	info("<<< exit >>>");

	return 0;
}

