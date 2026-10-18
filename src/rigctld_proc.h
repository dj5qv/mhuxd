/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef RIGCTLD_PROC_H
#define RIGCTLD_PROC_H

#include <stdint.h>

struct app_ctx;
struct mh_control;

/* A rigctld that mhuxd runs while the keyer is online, independent of the
 * rig mode sync backend. */
struct rigctld_proc;

struct rigctld_proc_cfg {
	const char *serial;
	uint8_t radio;
	int port;		/* rigctld -t */
	const char *options;	/* further rigctld arguments, split at whitespace */
};

struct rigctld_proc *rigctld_proc_create(struct app_ctx *ctx, struct mh_control *ctl,
					 const struct rigctld_proc_cfg *cfg);
void rigctld_proc_destroy(struct rigctld_proc *proc);

#endif // RIGCTLD_PROC_H
