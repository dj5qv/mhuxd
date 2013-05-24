/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "util.h"
#include "logger.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1


void *w_calloc(size_t nmemb, size_t size) {
	void *p;
	do {
		p = calloc(nmemb, size);
		if(!p)
			sleep(1);
	} while(!p);
	return p;
}

void *w_malloc(size_t size) {
	void *p;
	do {
		p = malloc(size);
		if(!p)
			sleep(1);
	} while(!p);
	return p;
}

char *w_strdup(const char *src) {
	return strdup(src);

	char *n = w_malloc(strlen(src) + 1);
	strcpy(n, src);
	return n;
}

void daemonize(void) {
	pid_t pid, sid;

	pid = fork();

	if (pid < 0) {
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
		exit(EXIT_FAILURE);
	}

	/* Redirect standard files to /dev/null */
        if(NULL == freopen( "/dev/null", "r", stdin)) {
                err_e(-errno, "freopen stdin failed, exiting!");
                exit(EXIT_FAILURE);
        }
        if(NULL == freopen( "/dev/null", "w", stdout)) {
                err_e(-errno, "freopen stdout failed, exiting!");
                exit(EXIT_FAILURE);
        }
        if(NULL == freopen( "/dev/null", "w", stderr)) {
                err_e(-errno, "freopen stdin failed, exiting!");
                exit(EXIT_FAILURE);
        }
}

