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
// Remove a keyer, which must be disconnected. Anything else referencing it (connectors, winkey manager
// users, rigctld clients) must have been destroyed before.
// Returns 0 on success, -ENOENT if not known, -EBUSY if still connected.
int dmgr_remove_device(struct device_manager *dmgr, const char *serial);
void dmgr_destroy(struct device_manager *dmgr);
struct device *dmgr_get_device(struct device_manager *dmgr, const char *serial);
struct PGList *dmgr_get_device_list(struct device_manager *dmgr);
// The name to show for a keyer: its custom name, by default the serial number.
const char *dmgr_device_name(const struct device *dev);
// Check a name for the keyer with this serial number. NULL, an empty name or the keyer's own serial
// number stand for the default name. Names and serial numbers of other keyers are taken, ignoring case.
// Returns 0 if the name can be used, -EINVAL if it is not valid, -EEXIST if it is taken.
int dmgr_check_device_name(struct device_manager *dmgr, const char *serial, const char *name);
// Set a name that passed dmgr_check_device_name().
void dmgr_set_device_name(struct device *dev, const char *name);

typedef void (*dmgr_device_cb)(struct device *dev, void *user_data);
t_on_device_connect_cb_handle *dmgr_add_on_device_connect_cb(struct device_manager *dmgr, dmgr_device_cb cb, void *user_data);
void dmgr_rem_on_device_connect_cb(struct device_manager *dmgr, t_on_device_connect_cb_handle *handle);


#endif // DEVMGR_H
