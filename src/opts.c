/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <fuse_opt.h>
#include "global.h"
#include "opts.h"
#include "logger.h"
#include "radiotypes.h"
#include "linux_udev.h"
#include "util.h"

static const char *usage =
"usage: mhux [options]\n"
"\n"
"options:\n"
" -h --help            print this help message\n"
" -c --cfg=CFGFILE     load config from this file\n"
#if 0
" -d --dev=TTYDEV      microham device by device node (e.g.: /dev/ttyUSB0)\n"
" -s --serial=SERIAL   microham device by serial no (e.g.: D2U12345)\n"
#endif
" -l --log-level=LEVEL log level (MUTE, CRIT, ERROR, WARN, INFO, DEBUG0, DEBUG1)\n"
" -b --background      Run in background as daemon. Log to syslog.\n"
"    --list-radios     Print a list of radios / codes\n"
"    --list-dev        List available microHam devices\n"
"\n";

#define MHUX_OPT(t, p) { t, offsetof(struct mhux_param, p), 1 }

static const struct fuse_opt opts[] = {
	FUSE_OPT_KEY("-h",              0),
        FUSE_OPT_KEY("--help",          0),

	MHUX_OPT("-c %s",               cfg_file),
	MHUX_OPT("--cfg=%s",            cfg_file),
#if 0
	MHUX_OPT("-d %s",               dev),
	MHUX_OPT("--dev=%s",            dev),

	MHUX_OPT("-s %s",               serial),
	MHUX_OPT("--serial=%s",         serial),
#endif
	MHUX_OPT("-l %s",               log_level),
	MHUX_OPT("--log-level=%s",      log_level),

	FUSE_OPT_KEY("-b",              2),
	FUSE_OPT_KEY("--background",    2),
	FUSE_OPT_KEY("--list-radios",   3),
	FUSE_OPT_KEY("--list-dev",      4),
        FUSE_OPT_END
};

#define DFL_CFG_FILE     "/etc/mhuxd.conf"
#define DFL_LOG_LEVEL    "INFO"

struct mhux_param mhux_params;

static int process_arg(void *data, const char * UNUSED(arg), int key,
                       struct fuse_args * UNUSED(outargs))
{
        struct mhux_param *param = data;

        switch (key) {
        case 0:
		fputs(usage, stderr);
                param->help = 1;
                return 0;
	case 2:
		param->as_daemon = 1;
		return 0;
	case 3:
		rtyp_print_types(stdout);
		param->help = 1;
		return 0;
	case 4:
		udv_print_device_list(stdout);
		param->help = 1;
		return 0;
	}
        return 1;
}

static struct fuse_args args;

int opt_parse(int argc, char **argv) {

	args.argc = argc;
	args.argv = argv;
	args.allocated = 0;

	if(fuse_opt_parse(&args, &mhux_params, opts, process_arg))
                return -1;

	if(!mhux_params.cfg_file)
		mhux_params.cfg_file = w_strdup(DFL_CFG_FILE);

	if(!mhux_params.log_level)
		mhux_params.log_level = w_strdup(DFL_LOG_LEVEL);

        return 0;
}

void opt_free() {
	fuse_opt_free_args(&args);

	/* Keep Valgrind happy */
#if 0
	if(mhux_params.dev)
		free((void*)mhux_params.dev);
	if(mhux_params.serial)
		free((void*)mhux_params.serial);
	mhux_params.dev = NULL;
	mhux_params.serial = NULL;
#endif
	if(mhux_params.cfg_file)
		free((void*)mhux_params.cfg_file);
	if(mhux_params.log_level)
		free((void*)mhux_params.log_level);

	mhux_params.cfg_file = NULL;
}
