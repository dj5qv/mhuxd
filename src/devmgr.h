/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef DEVMGR_H
#define DEVMGR_H

#include <stdint.h>
#include "pglist.h"
#include "mhinfo.h"

typedef struct on_device_connect_cb t_on_device_connect_cb_handle;

struct device {
	struct PGNode node;
	struct mh_router *router;
	struct mh_control *ctl;
	struct wkman *wkman;
	char *serial;
	struct mh_info mhi;
};

struct ev_loop;

void *dmgr_create(struct ev_loop *loop);
void dmgr_enable_monitor();
void dmgr_disable_monitor();
struct device *dmgr_add_device(const char *serial, uint16_t type);
void dmgr_destroy();
struct device *dmgr_get_device(const char *serial);
struct PGList *dmgr_get_device_list();

typedef void (*dmgr_device_cb)(struct device *dev, void *user_data);
t_on_device_connect_cb_handle *dmgr_add_on_device_connect_cb(dmgr_device_cb cb, void *user_data);
void dmgr_rem_on_device_connect_cb(t_on_device_connect_cb_handle *handle);

#endif // DEVMGR_H
