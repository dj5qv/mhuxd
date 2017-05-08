/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2017  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef PROC_ROTATOR_H
#define PROC_ROTATOR_H

struct buffer;
struct mh_router;
struct mh_control;
struct proc_rotator;

struct proc_rotator *rot_create(struct mh_control *ctl, int channel);
void rot_destroy(struct proc_rotator *rot);
void rot_cb(struct mh_router *router, struct buffer *b, void *user_data);

#endif // PROC_ROTATOR_H
