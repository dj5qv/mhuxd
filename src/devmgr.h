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
#include "device.h"
#include "pglist.h"
#include "mhinfo.h"

typedef struct on_device_connect_cb t_on_device_connect_cb_handle;

struct ev_loop;
typedef struct eventbus eventbus_t;

struct device_manager *dmgr_create(struct ev_loop *loop, eventbus_t *ebus);
void dmgr_enable_monitor(struct device_manager *dmgr);
void dmgr_disable_monitor(struct device_manager *dmgr);
struct device *dmgr_add_device(struct device_manager *dmgr, const char *serial);
void dmgr_destroy(struct device_manager *dmgr);
struct device *dmgr_get_device(struct device_manager *dmgr, const char *serial);
struct PGList *dmgr_get_device_list(struct device_manager *dmgr);

typedef void (*dmgr_device_cb)(struct device *dev, void *user_data);
t_on_device_connect_cb_handle *dmgr_add_on_device_connect_cb(struct device_manager *dmgr, dmgr_device_cb cb, void *user_data);
void dmgr_rem_on_device_connect_cb(struct device_manager *dmgr, t_on_device_connect_cb_handle *handle);


#endif // DEVMGR_H
