/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum {
	LOGSV_MUTE,
	LOGSV_CRIT,
        LOGSV_ERR,
        LOGSV_WARN,
        LOGSV_INFO,
        LOGSV_DFL    = LOGSV_INFO, /* default log level */
        LOGSV_DBG0,
        LOGSV_DBG1,
        LOGSV_MAX    = LOGSV_DBG1,
};

extern int log_level;

inline static int log_get_level() {
	return log_level;
}

void log_init(FILE *f);
void log_set_ident(const char *);
void log_set_level(int level);
int log_set_level_by_str(const char *s);
const char *log_get_level_str();
void log_hex(int severity, const char *msg1, const char *msg2, const char *msg3, const char *buf, int len);
void log_msg(int severity, const char *header, const char *fmt, ...)
#ifdef __GNUC__
     __attribute__ ((format (printf, 3, 4)));
#else
     ;
#endif

#define fatal(fmt, args...) do {                                        \
	log_msg(LOGSV_CRIT, "("MOD_ID")", fmt"\n" , ##args);	\
        _exit(1);                                                       \
} while (0)
#define err(fmt, args...)               log_msg(LOGSV_ERR,  "("MOD_ID")", fmt"\n" , ##args)
#define warn(fmt, args...)              log_msg(LOGSV_WARN, "("MOD_ID")", fmt"\n" , ##args)
#define info(fmt, args...)              log_msg(LOGSV_INFO, "("MOD_ID")", fmt"\n" , ##args)
#define dbg0(fmt, args...)            log_msg(LOGSV_DBG0, "("MOD_ID")", fmt"\n", ##args)
#define dbg1(fmt, args...)            log_msg(LOGSV_DBG1, "("MOD_ID")", fmt"\n", ##args)
#define dbg1_h(msg1, msg2, buf, len)   log_hex(LOGSV_DBG1, "(" MOD_ID ")", msg1, msg2, (const char*) buf, len)
#define fatal_e(e, fmt, args...)        \
        fatal(fmt" (%s)" , ##args, strerror((e))
#define err_e(e, fmt, args...)  \
        err(fmt" (%s)" , ##args, strerror(e))
#define warn_e(e, fmt, args...) \
        warn(fmt" (%s)" , ##args, strerror(e))
#define info_e(e, fmt, args...) \
        info(fmt" (%s)" , ##args, strerror(e))
#define dbg0_e(e, fmt, args...) \
        dbg0(fmt" (%s)" , ##args, strerror(e))
#define dbg1_e(e, fmt, args...) \
        dbg1(fmt" (%s)" , ##args, strerror(e))


#endif // LOGGER_H
