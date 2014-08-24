/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef DEVMGR_H
#define DEVMGR_H

#include <stdint.h>
#include "pglist.h"

struct device {
	struct PGNode node;
	struct mh_router *router;
	struct mh_control *ctl;
	struct sm *sm;
	struct wkman *wkman;
	char *serial;
};

struct ev_loop;
struct cfgmgr;

void *dmgr_create(struct ev_loop *loop, struct cfgmgr *cfgmgr);
void dmgr_enable_monitor();
struct device *dmgr_add_device(const char *serial, uint16_t type);
void dmgr_destroy();
struct device *dmgr_get_device(const char *serial);
struct PGList *dmgr_get_device_list();
struct cfgmgr *dmgr_get_cfgmgr();

#endif // DEVMGR_H
