/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CON_VSP_H
#define CON_VSP_H

/* Longest accepted VSP device name. The hard limit imposed by the buffers in
   vsp_create() is 112 characters; this is simply a sane device name length. */
#define VSP_DEVNAME_MAX 64

struct connector_spec;
struct vsp *vsp_create(const struct connector_spec *cpsec);
void vsp_destroy(struct vsp *vsp);

/* True if devname may be used as the leaf of /dev/mhuxd/<devname>. */
int vsp_devname_is_valid(const char *devname);

#endif // CON_VSP_H



