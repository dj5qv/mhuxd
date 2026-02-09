/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <errno.h>
#include <ev.h>

#include "devmgr.h"
#include "util.h"
#include "pglist.h"
#include "logger.h"
#include "tty.h"
#include "linux_udev.h"
#include "mhrouter.h"
#include "mhcontrol.h"
#include "mhinfo.h"
#include "wkman.h"

#define MOD_ID "dmr"

struct device_manager {
	struct PGList device_list;
	struct ev_loop *loop;
	struct devmon *devmon;
};

struct device_manager *dman = NULL;

static struct device *create_dev(const char *serial, uint16_t type) {
	dbg1("%s %s()", serial, __func__);

	struct device *dev = dmgr_get_device(serial);
	if(dev)
		return dev;

	if(!type) {
		type = mhi_type_from_serial(serial);
		if(type == MHT_UNKNOWN) {
			err("Could not determine keyer type from serial number (%s)", serial);
			return NULL;
		}
	}

	struct mh_info mhi;
	mhi_init(&mhi, type);
	if(!(mhi.flags & MHF_MHUXD_SUPPORTED)) {
		info("Ignoring unsupported device %s (%s)", serial, mhi.type_str);
		return NULL;
	}

	dev = w_calloc(1, sizeof(*dev));
	dev->serial = w_malloc(strlen(serial)+1);
	strcpy(dev->serial, serial);
	dev->router = mhr_create(dman->loop, serial, (mhi.flags & MHF_HAS_FLAGS_CHANNEL) ? 1 : 0);
	dev->ctl = mhc_create(dman->loop, dev->router, &mhi);
	PG_AddTail(&dman->device_list, &dev->node);
	return dev;
}

struct device *dmgr_add_device(const char *serial, uint16_t type) {
	return create_dev(serial, type);
}

static void devmon_callback(const char *serial, int status, void *user_data) {
	(void) user_data;


	dbg1("%s() status: %d", __func__, status);
	//	printf("device changed: %s - %d\n", serial, status);

	if(status == DEVMON_DISCONNECTED) {
		info("%s disconnected from USB", serial);
		return;
	}

	if(status != DEVMON_CONNECTED)
		return;

	if(MHT_UNKNOWN == mhi_type_from_serial(serial)) {
		err("Could not determine keyer type from serial number (%s)", serial);
		err("Or keyer not supported!");
		return;
	}

	const char *devnode = udv_dev_by_serial(serial);
	if(devnode == NULL) {
		err("could not determine device node for %s!", serial);
		return;
	}

	info("%s connected to USB on %s", serial, devnode);

	int fd = tty_open(devnode);
	if(fd == -1) {
		err_e(errno, "could not open device %s!", devnode);
		free((void*)devnode);
		return;
	}
	dbg0("opened %s", devnode);

	struct device *dev = create_dev(serial, MHT_UNKNOWN);

	if(dev)
		mhr_set_keyer_fd(dev->router, fd);

	free((void*)devnode);
}

void *dmgr_create(struct ev_loop *loop) {

	if(dman != NULL) {
		// this is a singleton
		return NULL;
	}

	dman = w_calloc(1, sizeof(*dman));
	PG_NewList(&dman->device_list);
	dman->loop = loop;
	return dman;
}

void dmgr_enable_monitor() {
	if(dman->devmon) {
		dbg0("%s() monitor already enabled!", __func__);
		return;
	}
	dman->devmon = devmon_create(dman->loop, devmon_callback, dman);
}

void dmgr_destroy() {
	struct device *dev;

	if(dman == NULL)
		return;

	while((dev = (void*)PG_FIRSTENTRY(&dman->device_list))) {
		wkm_destroy(dev->wkman);
		mhc_destroy(dev->ctl);
		mhr_destroy(dev->router);
		PG_Remove(&dev->node);
		if(dev->serial)
			free(dev->serial);
		free(dev);
	}

	devmon_destroy(dman->devmon);
	free(dman);
	dman = NULL;
}

struct PGList *dmgr_get_device_list() {
	if(dman == NULL)
		return NULL;
	return &dman->device_list;
}

struct device *dmgr_get_device(const char *serial) {
	struct device *d;
	PG_SCANLIST(&dman->device_list, d) {
		if(!strcmp(d->serial, serial))
			return d;
	}
	return NULL;
}

