/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CTL_H
#define CTL_H

struct ctl;
struct dispatcher;
struct cmd_request;
struct config;

struct ctl *ctl_create(struct dispatcher *dp, struct config *cfg);
void ctl_destroy(struct ctl *ctl);
int ctl_init(struct ctl *ctl);
int ctl_probe(struct ctl *ctl);
void ctl_set_cfg(struct ctl *ctl, struct config *cfg);
const struct mh_info *ctl_get_mhinfo(struct ctl *ctl);

#endif // CTL_H
