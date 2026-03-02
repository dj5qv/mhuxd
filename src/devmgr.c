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
#include "wkman.h"

#define MOD_ID "dmr"

typedef struct on_device_connect_cb {
	struct PGNode node;
	dmgr_device_cb cb;
	void *user_data;
} t_on_device_connect_cb_handle;

struct device_manager {
	struct PGList device_list;
	struct PGList on_device_connect_cbs;
	struct ev_loop *loop;
	struct devmon *devmon;
};

static struct device *create_dev(struct device_manager *dmgr, const char *serial) {
	dbg1("%s %s()", serial, __func__);
	uint8_t type = mhi_type_from_serial(serial);

	// Device already there?
	struct device *dev = dmgr_get_device(dmgr, serial);
	if(dev)
		return dev;

	if(type == MHT_UNKNOWN) {
		err("Could not determine keyer type from serial number (%s)", serial);
		return NULL;
	}

	struct mh_info mhi;
	mhi_init(&mhi, type);
	if(!(mhi.flags & MHF_MHUXD_SUPPORTED)) {
		info("Ignoring unsupported device %s (%s)", serial, mhi.type_str);
		return NULL;
	}

	dev = w_calloc(1, sizeof(*dev));
	memcpy(&dev->mhi, &mhi, sizeof(mhi));
	dev->serial = w_malloc(strlen(serial)+1);
	strcpy(dev->serial, serial);
	dev->router = mhr_create(dmgr->loop, serial, (mhi.flags & MHF_HAS_FLAGS_CHANNEL) ? 1 : 0);
	dev->ctl = mhc_create(dmgr->loop, dev->router, &mhi);
	PG_AddTail(&dmgr->device_list, &dev->node);

	t_on_device_connect_cb_handle *odccb;
	PG_SCANLIST(&dmgr->on_device_connect_cbs, odccb) {
		if(odccb->cb)			
			odccb->cb(dev, odccb->user_data);
	}

	return dev;
}

struct device *dmgr_add_device(struct device_manager *dmgr, const char *serial) {
	return create_dev(dmgr, serial);
}

static void devmon_callback(const char *serial, int status, void *user_data) {
	struct device_manager *dmgr = (struct device_manager *)user_data;


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

	struct device *dev = create_dev(dmgr, serial);

	if(dev)
		mhr_set_keyer_fd(dev->router, fd);

	free((void*)devnode);
}

struct device_manager *dmgr_create(struct ev_loop *loop) {
	struct device_manager *dmgr = w_calloc(1, sizeof(*dmgr));
	PG_NewList(&dmgr->device_list);
	PG_NewList(&dmgr->on_device_connect_cbs);
	dmgr->loop = loop;
	return dmgr;
}

void dmgr_enable_monitor(struct device_manager *dmgr) {
	if(!dmgr || !dmgr->loop) return;
	if(dmgr->devmon) {
		dbg0("%s() monitor already enabled!", __func__);
		return;
	}
	dmgr->devmon = devmon_create(dmgr->loop, devmon_callback, dmgr);
}

void dmgr_disable_monitor(struct device_manager *dmgr) {
	if(!dmgr->devmon) {
		dbg0("%s() monitor not enabled!", __func__);
		return;
	}
	devmon_destroy(dmgr->devmon);
	dmgr->devmon = NULL;
}

void dmgr_destroy(struct device_manager *dmgr) {
	struct device *dev;

	if(dmgr == NULL)
		return;

	t_on_device_connect_cb_handle *odccb;

	while((odccb = (void*)PG_FIRSTENTRY(&dmgr->on_device_connect_cbs))) {
		PG_Remove(&odccb->node);
		free(odccb);
	}

	while((dev = (void*)PG_FIRSTENTRY(&dmgr->device_list))) {
		wkm_destroy(dev->wkman);
		mhc_destroy(dev->ctl);
		mhr_destroy(dev->router);
		PG_Remove(&dev->node);
		if(dev->serial)
			free(dev->serial);
		free(dev);
	}

	if (dmgr->devmon)
		devmon_destroy(dmgr->devmon);

	free(dmgr);
	dmgr = NULL;
}

struct PGList *dmgr_get_device_list(struct device_manager *dmgr) {
	if(dmgr == NULL)
		return NULL;
	return &dmgr->device_list;
}

struct device *dmgr_get_device(struct device_manager *dmgr, const char *serial) {
	struct device *d;
	PG_SCANLIST(&dmgr->device_list, d) {
		if(!strcmp(d->serial, serial))
			return d;
	}
	return NULL;
}

t_on_device_connect_cb_handle *dmgr_add_on_device_connect_cb(struct device_manager *dmgr, dmgr_device_cb cb, void *user_data) {
	struct on_device_connect_cb *odccb = w_calloc(1, sizeof(*odccb));
	odccb->cb = cb;
	odccb->user_data = user_data;
	PG_AddTail(&dmgr->on_device_connect_cbs, &odccb->node);
	return odccb;
}

void dmgr_rem_on_device_connect_cb(struct device_manager *dmgr, t_on_device_connect_cb_handle *handle) {
	if(!handle) return;
	t_on_device_connect_cb_handle *odccb;
	PG_SCANLIST(&dmgr->on_device_connect_cbs, odccb) {
		if(odccb == handle) {
			PG_Remove(&odccb->node);
			free(odccb);
			return;
		}
	}
	warn("%s() callback handle not found!", __func__);
}