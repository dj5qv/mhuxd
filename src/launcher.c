/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include "launcher.h"
#include "util.h"
#include "logger.h"
#include "opts.h"
#include "tty.h"
#include "dispatcher.h"
#include "ctl.h"
#include "vsp.h"
#include "cfgfile.h"
#include "linux_udev.h"


struct launcher {
	struct dispatcher *dp;
	struct ctl        *ctl;
	struct config     *cfg;
	FILE		  *pidfile;
	const char        *pidfile_name;
	int    fd;
};

static FILE *pidfile_lock(const char *name) {
	int fd;
	int pid;
	FILE *f;

	fd = open(name, O_RDWR | O_CREAT, 0644);

	if(fd == -1) {
		err_e(-errno, "Could not open or create pidfile %s!", name);
		return NULL;
	}

	f = fdopen(fd, "r+");

	if(f == NULL) {
		err_e(-errno, "Could not open pidfile %s!", name);
		close(fd);
		return NULL;
	}

	if(flock(fd, LOCK_EX | LOCK_NB)) {
		err_e(-errno, "Could not lock pidfile %s!", name);
		err("Daemon already running?");

		fclose(f);
		return NULL;
	}

	pid = getpid();

	if(fprintf(f, "%d\n", pid) <= 0) {
		err("Could not write to pidfile %s!", name);
		fclose(f);
		return NULL;
	}

	if(fflush(f)) {
		err_e(-errno,"Could not write to pidfile %s!", name);
		fclose(f);
		return NULL;
	}

	return f;
}

static void pidfile_unlock(FILE *f, const char *name) {
	if(f)
		fclose(f);
	if(name)
		unlink(name);
}


static void hup_handler(void *data) {
	struct launcher *lc = (struct launcher *)data;
	struct config *new_cfg = cfg_load(mhux_params.cfg_file);

	if(!new_cfg)
		return;

	if(lc->cfg)
		cfg_destroy(lc->cfg);

	lc->cfg = new_cfg;

	if(lc->ctl)
		ctl_set_cfg(lc->ctl, lc->cfg);
}

static const char *serial2device(const char *serial, int retry) {
	const char *dev;

	do {
		dev = udv_dev_by_serial(serial);
		if(!dev && !retry)
			err("Could not determine device path from serial number '%s'", serial);
		if(!dev && !sig_term) {
			dbg0("Could not determine device path from serial number '%s'", serial);
			sleep(3);
		}

	} while(!dev && retry && !sig_term);

	if(dev)
		info("Determined device path %s from serial number %s",
		dev, serial);

	return dev;
}

static int wait_for_device(struct launcher *lc, int retry) {
	int err;

	do {
		lc->dp  = dp_create();
		lc->ctl = ctl_create(lc->dp, lc->cfg);
		dp_set_dev_timeout(lc->dp, cfg_get_int(lc->cfg, "//Daemon/DeviceTimeout", 500));

		do {
			if(sig_term)
				exit(-1);

			const char *dev = w_strdup(cfg_get_str(lc->cfg, "//Daemon/Device", "*"));


			if(*dev != '/')
				dev = serial2device(dev, retry);

			lc->fd = tty_open(dev);

			if(lc->fd == -1) {
				if(dev)
					err_e(-errno, "LAUNCH Could not open device %s!", dev);

				if(retry && !sig_term)
					sleep(2);

			} else {
				info("LAUNCH Device %s opened!", dev);
			}

			if(dev)
				free((void*)dev);

		} while(retry && lc->fd == -1 && !sig_term);

		err = -1;

		if(lc->fd >= 0) {

			dp_set_keyer_fd(lc->dp, lc->fd);
			err = ctl_probe(lc->ctl);
			if(err) {
				err("LAUNCH Device does not respond, keyer switched off?");
				if(retry && !sig_term)
					sleep(2);
			}
		}

		if(!err)
			return 0;

		ctl_destroy(lc->ctl);
		dp_destroy(lc->dp);

		lc->dp = NULL;
		lc->ctl = NULL;

	} while(retry && !sig_term);

	return -1;
}

struct launcher * launch_start_all() {
	struct launcher *lc;
	int err, retry;
	const char *loglevel;

	lc = w_calloc(1, sizeof(*lc));
	lc->fd = -1;

	lc->cfg = cfg_load(mhux_params.cfg_file);
	if(!lc->cfg)
		fatal("Could not load config!");

	lc->pidfile_name = w_strdup(cfg_get_str(lc->cfg, "//Daemon/PidFile", "/var/run/_foo.pid"));

	lc->pidfile = pidfile_lock(lc->pidfile_name);

	if( ! lc->pidfile)
		return lc;

	if((loglevel = cfg_get_str(lc->cfg, "//Daemon/LogLevel", NULL))) {
		dbg0("LAUNCH Loglevel from cfg: %s", loglevel);
	} else {
		loglevel = mhux_params.log_level;
		dbg0("LAUNCH LogLevel from cmd line: %s", loglevel);
	}

	if(log_set_level_by_str(loglevel) < 0)
		err("LAUNCH Invalid LogLevel: %s", loglevel);

	retry = cfg_get_bool(lc->cfg, "//Daemon/DeviceRetry", 0);

	do {
		err = wait_for_device(lc, 0);

		if(err && retry) {
			info("LAUNCH Waiting for device now");
			log_set_level(LOGSV_MUTE);
			err = wait_for_device(lc, 1);
			log_set_level_by_str(loglevel);
		}

		if(!err) {
			err = ctl_init(lc->ctl);
			if(!err) {
				//struct cfgmon *cfgmon = cfgmon_create(mhux_params.cfg_file, cfgfile_changed);
				dp_set_hup_handler(hup_handler, lc);
				dp_loop(lc->dp, 0);
				dp_set_hup_handler(NULL, NULL);
				// cfgmon_destroy(cfgmon);
			} else {
				err("LAUNCH Could not initialize device!");
			}
		}

		ctl_destroy(lc->ctl);
		dp_destroy(lc->dp);

		lc->dp = NULL;
		lc->ctl = NULL;

	} while(retry && !sig_term);

	return lc;
}

void launch_stop_all(struct launcher *lc) {
	if(!lc)
		return;
	if(lc->ctl)
		ctl_destroy(lc->ctl);
	if(lc->dp)
		dp_destroy(lc->dp);
	if(lc->cfg)
		cfg_destroy(lc->cfg);
	if(lc->pidfile)
		pidfile_unlock(lc->pidfile, lc->pidfile_name);

	free((void*)lc->pidfile_name);
	free(lc);
}


void launch_loop(struct launcher *lc) {
	if(lc && lc->dp)
		dp_loop(lc->dp, 0);
}
