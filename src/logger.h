/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
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

void log_init(FILE *f);
void log_set_ident(const char *);
void log_set_level(int level);
int log_set_level_by_str(const char *s);
int log_get_level();
const char *log_get_level_str();
void log_hex(int severity, const char *header, const char *buf, int len);
void log_msg(int severity, const char *fmt, ...)
#ifdef __GNUC__
     __attribute__ ((format (printf, 2, 3)));
#else
     ;
#endif



#define fatal(fmt, args...) do {                                        \
	log_msg(LOGSV_CRIT, fmt"\n" , ##args);                              \
        _exit(1);                                                       \
} while (0)
#define err(fmt, args...)               log_msg(LOGSV_ERR,  fmt"\n" , ##args)
#define warn(fmt, args...)              log_msg(LOGSV_WARN, fmt"\n" , ##args)
#define info(fmt, args...)              log_msg(LOGSV_INFO, fmt"\n" , ##args)
#define dbg0(fmt, args...)              log_msg(LOGSV_DBG0, fmt"\n" , ##args)
#define dbg1(fmt, args...)              log_msg(LOGSV_DBG1, fmt"\n" , ##args)

#define dbg0_h(hdr, buf, len)           log_hex(LOGSV_DBG0, hdr, (const char*) buf, len)
#define dbg1_h(hdr, buf, len)           log_hex(LOGSV_DBG1, hdr, (const char*) buf, len)

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
