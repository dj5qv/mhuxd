/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include "logger.h"

#define MOD_ID "log"

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

int log_level = LOGSV_DFL;
static FILE *file = NULL;

static int current_time(char buf[32]) {
	struct timeval tv;
	struct tm tm;
	int r;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);

	r = snprintf(buf, 32, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
		 tm.tm_year + 1900,
		 tm.tm_mon + 1,
		 tm.tm_mday,
		 tm.tm_hour,
		 tm.tm_min,
		 tm.tm_sec,
		 tv.tv_usec / 1000
		 );

	return r<=0;
}

void log_init(FILE *f) {
	file = f;
}

void log_set_level(int level) {
	if(level >= 0 && level <= LOGSV_MAX) {
		log_level = level;
		info("log level changed to %s", level_map[level].name);
	}
}

int log_set_level_by_str(const char *s) {
	uint16_t i;
	if(!s)
		return -1;
	for(i = 0; i < sizeof(level_map) / sizeof(struct level); i++) {
		if(!strcasecmp(level_map[i].name, s)) {
			log_set_level(level_map[i].level);
			return i;
		}
	}
	return -1;
}

const char *log_get_level_str() {
	uint16_t i;
	for(i = 0; i < sizeof(level_map) / sizeof(struct level); i++) {
		if(level_map[i].level == log_level)
			return level_map[i].name;
	}
	return "INVALID";
}

static const char *severity_strs[] = {
        [LOGSV_CRIT]         = "CRIT",
	[LOGSV_ERR]          = "ERR ",
        [LOGSV_WARN]         = "WARN",
        [LOGSV_INFO]         = "INFO",
        [LOGSV_DBG0]         = "DBG0",
        [LOGSV_DBG1]         = "DBG1",
};

void log_hex(int severity, const char *msg1, const char *msg2, const char *msg3, const char *buf, int len) {
	int c_per_line;
	const char *p;

	if(severity > log_level)
		return;

	if(msg1 == NULL)
		msg1 = "";
	if(msg2 == NULL)
		msg2 = "";
	if(msg3 == NULL)
		msg3 = "";

	if(file == NULL || severity < 0 || severity > LOGSV_MAX || len <= 0)
		return;

	c_per_line = 81 / 3;

	for(p = buf; p < buf + len; p++) {
		if(!((p - buf) % c_per_line)) {
			if(p > buf)
				fputs("\n", file);
			log_msg(severity, msg1, "%s%s%s:", msg2, *msg2 ? " " : "",  msg3);
		}
		fprintf(file, " %02x", (unsigned char)*p);
	}
	if(len)
		fputs("\n", file);

	fflush(file);

}

void log_msg(int severity, const char *header, const char *fmt, ...) {
	char buf[32];

	if(severity > log_level)
		return;

	if(file == NULL || severity < 0 || severity > LOGSV_MAX)
		return;

	if(header == NULL)
		header = "";
	
	if(current_time(buf))
		return;

	fprintf(file, "%s %s %s ", buf, severity_strs[severity], header);

	va_list vl;
	va_start(vl, fmt);
	vfprintf(file, fmt, vl);
	va_end(vl);

	fflush(file);
}

