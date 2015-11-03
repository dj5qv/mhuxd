/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "daemon.h"
#include "logger.h"
#include "cfgnod.h"

#define MOD_ID "dmn"

int16_t dmn_set(struct cfg *cfg) {
	const char *p;

	if((p = cfg_get_val(cfg, "LOGLEVEL", NULL))) {
		if(-1 == log_set_level_by_str(p)) {
			err("invalid value for loglevel: %s", p);
		}
	}

	// atl_warn_unused(args, "dmn");

	return 0;
}

void dmn_daemonize(void) {
	pid_t pid, sid;

	pid = fork();

	if (pid < 0) {
		err_e(errno, "Failed to fork, exiting!");
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	umask(0);

	sid = setsid();
	if (sid < 0) {
		exit(EXIT_FAILURE);
	}

	if ((chdir("/")) < 0) {
		err_e(errno, "Failed to change directory / , exiting!");
		exit(EXIT_FAILURE);
	}

	/* Redirect standard files to /dev/null */
	if(NULL == freopen( "/dev/null", "r", stdin)) {
		err_e(errno, "freopen stdin failed, exiting!");
		exit(EXIT_FAILURE);
	}
	if(NULL == freopen( "/dev/null", "w", stdout)) {
		err_e(errno, "freopen stdout failed, exiting!");
		exit(EXIT_FAILURE);
	}
	if(NULL == freopen( "/dev/null", "w", stderr)) {
		err_e(errno, "freopen stdin failed, exiting!");
		exit(EXIT_FAILURE);
	}
}


FILE *dmn_pidfile_lock(const char *name) {
        int fd;
        int pid;
        FILE *f;

        fd = open(name, O_RDWR | O_CREAT, 0644);

        if(fd == -1) {
                err_e(errno, "Could not open or create pidfile %s!", name);
                return NULL;
        }

        f = fdopen(fd, "r+");

        if(f == NULL) {
                err_e(errno, "Could not open pidfile %s!", name);
                close(fd);
                return NULL;
        }

        if(flock(fd, LOCK_EX | LOCK_NB)) {
                err_e(errno, "Could not lock pidfile %s!", name);
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
                err_e(errno,"Could not write to pidfile %s!", name);
                fclose(f);
                return NULL;
        }

        return f;
}

void dmn_pidfile_unlock(FILE *f, const char *name) {
        if(name)
                unlink(name);
        if(f)
                fclose(f);
}
