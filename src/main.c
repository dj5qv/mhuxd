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

#ifndef LOGDIR
#error LOGDIR not defined
#endif

#ifndef RUNDIR
#error RUNDIR not defined
#endif

#define LOGFILE LOGDIR "/mhuxd.log"
#define PIDFILE RUNDIR "/mhuxd.pid"

const char *log_file_name = LOGFILE;
static FILE *logfile = NULL;

static int signum = 0;
static void sigint_cb(struct ev_loop *loop, struct ev_signal *w, int revents)
{
	(void)w; (void)revents;
	signum = w->signum;
	ev_unloop (loop, EVUNLOOP_ALL);
}
static void sighup_cb(struct ev_loop *loop, struct ev_signal *w, int revents) {
	(void)w; (void)revents; (void)loop;
	if(logfile && logfile != stdout) {
		info("*** SIGHUP received-> closing log file");
		fclose(logfile);
		logfile = fopen(log_file_name, "a");
		log_init(logfile);
		info("*** logfile opened");
	}
}

int main(int argc, char **argv)
{
    struct ev_signal w_sigint, w_sigterm, w_sigpipe, w_sighup;
    struct ev_loop *loop;
	struct cfgmgr *cfgmgr;
	struct cfgmgrj *cfgmgrj; 
	struct conmgr *conmgr;
	FILE *pidfile = NULL;

	printf("\n%s (C)2012-2026 Matthias Moeller, DJ5QV\n", PACKAGE_STRING);

	// options
	process_opts(argc, argv);
	if(-1 == log_set_level_by_str(log_level_str)) // from opt.c
		fprintf(stderr, "Invalid log level: %s", log_level_str);

	// logging
	if(!background) {
		logfile = stdout;
	} else {
		logfile = fopen(log_file_name, "a");
		printf("Logfile is: %s\n", log_file_name);
	}

	if(logfile == NULL) {
		fprintf(stderr, "could not open logfile %s (%s)!\n", log_file_name, strerror(errno));
		return -1;
	}

	log_init(logfile);

	if(background) {
		dmn_daemonize();
	}

	info("%s (C)2012-2026 Matthias Moeller, DJ5QV", PACKAGE_STRING);
	info("Logfile: %s", log_file_name);

	// pid file
	pidfile = dmn_pidfile_lock(PIDFILE);
	if(pidfile == NULL)
		return -1;

	// ev setup
	ev_set_allocator((void*)w_realloc);
	loop = ev_default_loop(EVFLAG_AUTO);

	// Connector manager
	conmgr = conmgr_create();

	// load config & apply the daemon part
	cfgmgr = cfgmgr_create(conmgr, loop);
	if(!cfgmgr) {
		fatal("(mhuxd) Could not create configuration manager, exiting!");
		exit(-1);
	}

	cfgmgrj = cfgmgrj_create(loop);
	if(!cfgmgrj) {
		fatal("(mhuxd) Could not create json manager, exiting!");
		exit(-1);
	}


	// start webserver & webui
	struct http_server *hs = hs_start(loop, webui_host_port);
	if(!hs) {
		fatal("(mhuxd) Could not start webserver, exiting!");
		exit(-1);
	}

	struct restapi *restapi = restapi_create(hs, cfgmgrj);
	if(!restapi) {
		fatal("(mhuxd) Could not start REST API, exiting!");
		hs_stop(hs);
		exit(-1);
	}

	struct webui *webui = webui_create(hs, cfgmgr);

	if(!webui) {
		fatal("(mhuxd) Could not start webui, exiting!");
		restapi_destroy(restapi);
		hs_stop(hs);
		exit(-1);
	}


	ev_signal_init (&w_sigint, sigint_cb, SIGINT);
	ev_signal_init (&w_sigterm, sigint_cb, SIGTERM);
	ev_signal_init (&w_sigpipe, sigint_cb, SIGPIPE);
	ev_signal_init (&w_sighup, sighup_cb, SIGHUP);

	ev_signal_start (loop, &w_sigint);
	ev_signal_start (loop, &w_sigterm);
	ev_signal_start (loop, &w_sigpipe);
	ev_signal_start (loop, &w_sighup);

	dmgr_create(loop, cfgmgr);

	if(cfgmgr_init(cfgmgr))
		err("(main) error initializing config manager!");

	dmgr_enable_monitor();

	if(demo_mode) {
		dmgr_add_device("CK_DEMO_CK_1", 0);
		dmgr_add_device("DK_DEMO_DK_1", 0);
		dmgr_add_device("D2_DEMO_DK2_1", 0);
		dmgr_add_device("MK_DEMO_MK_1", 0);
		dmgr_add_device("M2_DEMO_MK2_1", 0);
		dmgr_add_device("M3_DEMO_MK3_1", 0);
		dmgr_add_device("2R_DEMO_MK2R_1", 0);
		dmgr_add_device("2P_DEMO_MK2Rp_1", 0);
		dmgr_add_device("UR_DEMO_U2R_1", 0);
		dmgr_add_device("SM_DEMO_SM_1", 0);
		dmgr_add_device("SD_DEMO_SMD_1", 0);
	}

	ev_loop(loop, 0);

	if(signum) 
		info("*** %s received!", strsignal(signum));

	cfgmgr_save_cfg(cfgmgr);
	cfgmgrj_save_cfg(cfgmgrj);

	cfgmgr_destroy(cfgmgr);
	cfgmgrj_destroy(cfgmgrj);
	conmgr_destroy(conmgr);

	dmgr_destroy();
	
	webui_destroy(webui);
	restapi_destroy(restapi);
	hs_stop(hs);

	ev_default_destroy();

	dmn_pidfile_unlock(pidfile, PIDFILE);

	info("<<< exit >>>");

	if(logfile && logfile != stdout)
		fclose(logfile);

	nerr_free();

	return 0;
}

