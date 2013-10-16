/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
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
static FILE *file = NULL;

static int current_time(char buf[32]) {
	struct timeval tv;
	struct tm tm;
	int r;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	r = strftime(buf, 32, "%Y-%m-%d %H:%M:%S", &tm);
	return !r;
}

void log_init(FILE *f) {
	file = f;
}

void log_set_level(int level) {
	if(level >= 0 && level <= LOGSV_MAX) {
		log_level = level;
		info("(log) log level changed to %s", level_map[level].name);
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

void log_hex(int severity, const char *header, const char *buf, int len) {
	int c_per_line;
	const char *p;

	if(severity > log_level)
                return;

	if(file == NULL || severity < 0 || severity > LOGSV_MAX || len <= 0)
                return;

	if(!header)
		header = "HEX";

	c_per_line = (80 - strlen(header)) / 3;

	for(p = buf; p < buf + len; p++) {
		if(!((p - buf) % c_per_line)) {
			if(p > buf)
				fputs("\n", file);
			log_msg(severity, "%s:", header);
		}
		fprintf(file, " %02x", (unsigned char)*p);
	}
	if(len)
		fputs("\n", file);

	fflush(file);
}

void log_msg(int severity, const char *fmt, ...) {
	char buf[32];

	if(severity > log_level)
		return;

	if(file == NULL || severity < 0 || severity > LOGSV_MAX)
		return;

	if(current_time(buf))
		return;

	fprintf(file, "%s %s ", buf, severity_strs[severity]);

	va_list vl;
	va_start(vl, fmt);
	vfprintf(file, fmt, vl);
	va_end(vl);

	fflush(file);
}
