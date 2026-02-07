/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CON_VSP_H
#define CON_VSP_H

struct connector_spec;
struct vsp *vsp_create(const struct connector_spec *cpsec);
void vsp_destroy(struct vsp *vsp);

#endif // CON_VSP_H



