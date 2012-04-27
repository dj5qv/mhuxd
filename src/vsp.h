/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef VSP_H
#define VSP_H

enum {
	VSPFL_RTS_IS_PTT = (1<<0),
	VSPFL_DTR_IS_PTT = (1<<1),
};

struct vsp_config {
	char devname[512];
	int flags;
};

extern int vsp_create(const struct vsp_config *vcfg, int fd_data, int fd_ctl);
extern int vsp_destroy(const char *devname);
int vsp_destroy_all();


#endif // VSP_H
