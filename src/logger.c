/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include "logger.h"

#define IDENT_SIZE 16

struct level {
	const char *name;
	int level;
};

static struct level level_map[] = {
	{ "MUTE", LOGSV_MUTE },
	{ "CRIT", LOGSV_CRIT },
	{ "ERROR", LOGSV_ERR },
	{ "WARN", LOGSV_WARN },
	{ "INFO", LOGSV_INFO },
	{ "DEBUG0", LOGSV_DBG0 },
	{ "DEBUG1", LOGSV_DBG1 }
};

static int log_level = LOGSV_DFL;
static char ident[IDENT_SIZE];
static pthread_mutex_t		logger_mutex;
static pthread_mutexattr_t	logger_mutex_attr;

static int current_time(char buf[32]) {
	struct timeval tv;
	struct tm tm;
	int r;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	r = strftime(buf, 32, "%Y-%m-%d %H:%M:%S", &tm);
	return !r;
}

void log_init() {
	pthread_mutexattr_init(&logger_mutex_attr);
	pthread_mutexattr_settype(&logger_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&logger_mutex, &logger_mutex_attr);
}

void log_set_ident(const char *s) {
	if(!s)
		return;
	closelog();
	strncpy(ident, s, IDENT_SIZE - 1);
	ident[IDENT_SIZE - 1] = 0x00;
	openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
}

void log_set_level(int level) {
	if(level >= 0 && level <= LOGSV_MAX)
		log_level = level;
}

int log_set_level_by_str(const char *s) {
	unsigned i;
	for(i = 0; i < sizeof(level_map) / sizeof(struct level); i++) {
		if(!strcasecmp(level_map[i].name, s)) {
			log_set_level(level_map[i].level);
			return i;
		}
	}
	return -1;
}

static const char *severity_strs[] = {
        [LOGSV_CRIT]         = "CRIT",
	[LOGSV_ERR]          = "ERR ",
        [LOGSV_WARN]         = "WARN",
        [LOGSV_INFO]         = "INFO",
        [LOGSV_DBG0]         = "DBG0",
        [LOGSV_DBG1]         = "DBG1",
};

static int severity_map[] = {
	[LOGSV_CRIT]         = LOG_ERR,
	[LOGSV_ERR]          = LOG_ERR,
	[LOGSV_WARN]         = LOG_WARNING,
	[LOGSV_INFO]         = LOG_INFO,
	[LOGSV_DBG0]         = LOG_DEBUG,
	[LOGSV_DBG1]         = LOG_DEBUG,
};


static void log_hex_syslog(int severity, const char *header, const char *buf, int len) {
	int c_per_line;
	const char *pin;
	char *pout;
	char out[1024];

	out[1024-1] = 0x00;
	pout = out;
	c_per_line = (80 - strlen(header)) / 3;

	for(pin = buf; pin < buf + len; pin++) {
		if(pin > buf && (!(pin - buf) % c_per_line)) {
			syslog(severity_map[severity], "%s%s", header, out);
			pout = out;
		}
		pout += snprintf(pout, 1023 - (pout - out), " %02x", (unsigned char)*pin);
	}

	if(pout > out)
		syslog(severity_map[severity], "%s%s", header, out);

}

void log_hex(int severity, const char *header, const char *buf, int len) {
	int c_per_line;
	const char *p;

	if(severity > log_level)
                return;

	if(severity < 0 || severity > LOGSV_MAX || len <= 0)
                return;
 
	if(!header)
		header = "HEX";

	if(strlen(ident)) {
		log_hex_syslog(severity, header, buf, len);
		return;
	}

	c_per_line = (80 - strlen(header)) / 3;

	pthread_mutex_lock(&logger_mutex);


	for(p = buf; p < buf + len; p++) {
		if(!((p - buf) % c_per_line)) {
			if(p > buf)
				fputs("\n", stderr);
			log_msg(severity, "%s:", header);
		}
		fprintf(stderr, " %02x", (unsigned char)*p);
	}
	if(len)
		fputs("\n", stderr);

	pthread_mutex_unlock(&logger_mutex);

}

void log_msg(int severity, const char *fmt, ...) {

	if(severity > log_level)
		return;

	if(!stderr || severity < 0 || severity > LOGSV_MAX)
		return;

	pthread_mutex_lock(&logger_mutex);

	if(!strlen(ident)) {
		char buf[32];

		if(current_time(buf))
			goto out;

		fprintf(stderr, "%s %s ", buf, severity_strs[severity]);

		va_list vl;
		va_start(vl, fmt);
		vfprintf(stderr, fmt, vl);
		va_end(vl);
	} else {
		va_list vl;
		va_start(vl, fmt);
		vsyslog(severity_map[severity], fmt, vl);
		va_end(vl);
	}

	out:
	;
	pthread_mutex_unlock(&logger_mutex);
}
