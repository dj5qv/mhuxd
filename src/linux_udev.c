/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <libudev.h>
#include <ev.h>
#include "config.h"
#include "linux_udev.h"
#include "pglist.h"
#include "util.h"
#include "logger.h"

#define MOD_ID "udev"

struct devmon {
	devmon_cb devmon_cb;
	void *userdata;
	struct udev *udev;
	struct udev_monitor *mon;
	struct ev_loop *loop;
	struct ev_io w;
	int fd;
};

struct devinfo {
	struct PGNode node;
	char *name;
	char *serial;
	char *device;
};

static struct PGList *udv_get_device_list() {
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct PGList *l;

	l = w_malloc(sizeof(*l));
	PG_NewList(l);

	udev = udev_new();
	if (!udev) {
		err("Can't create udev\n");
		return l;
	}

	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "tty");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		struct udev_device *ddev, *pdev;
		const char *path;
		path = udev_list_entry_get_name(dev_list_entry);
		ddev = udev_device_new_from_syspath(udev, path);

		if(!ddev) {
			err("udev_device_new_from_syspath() failed!");
			continue;
		}

		pdev = udev_device_get_parent_with_subsystem_devtype(
								    ddev,
								    "usb",
								    "usb_device");

		if(!pdev || strcmp(udev_device_get_sysattr_value(pdev,"idVendor"), "0403")) {
			udev_device_unref(ddev);
			continue;
		}

		if(strcasecmp(udev_device_get_sysattr_value(pdev,"idProduct"), "EEEF")) {
			udev_device_unref(ddev);
			continue;
		}

		const char *manu = udev_device_get_sysattr_value(pdev,"manufacturer");
		const char *prod = udev_device_get_sysattr_value(pdev,"product");

		struct devinfo *di = w_calloc(1, sizeof(*di));
		di->device = w_strdup(udev_device_get_devnode(ddev));

		di->name = w_malloc(strlen(manu) + strlen(prod) + 3);
		*di->name = 0x00;
		sprintf(di->name, "%s %s", manu, prod);
		di->serial = w_strdup(udev_device_get_sysattr_value(pdev, "serial"));

		PG_AddTail(l, &di->node);

		udev_device_unref(ddev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	return l;
}

static void udv_free_device_list(struct PGList *l) {
	struct devinfo *di;
	while((di = (void*)PG_FIRSTENTRY(l))) {
		PG_Remove(&di->node);
		if(di->device)
			free(di->device);
		if(di->name)
			free(di->name);
		if(di->serial)
			free(di->serial);
		free(di);
	}
	free(l);
}

static void mon_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop; (void)revents;

	struct devmon *devmon = w->data;
	struct udev_device *dev, *pdev;

        dev = udev_monitor_receive_device(devmon->mon);
	if(dev == NULL)
		return;

	const char *type = udev_device_get_subsystem(dev);
	if(!type || strcmp(type, "tty"))
		goto out;

	pdev = udev_device_get_parent_with_subsystem_devtype(
							     dev,
							     "usb",
							     "usb_device");
	if(pdev == NULL)
		goto out;

	const char *vend = udev_device_get_sysattr_value(pdev,"idVendor");
	const char *prod = udev_device_get_sysattr_value(pdev,"idProduct");
	const char *serial = udev_device_get_sysattr_value(pdev,"serial");
	const char *action = udev_device_get_action(dev);
	dbg1("%s() action: %s pdev: %ld vend: %ld prod: %ld serial: %ld\n", __func__, action, (long)pdev, (long)vend, (long)prod, (long)serial);

	if(vend == NULL || serial == NULL)
		goto out;

	if(!strcmp(vend, "0403") && !strcasecmp(prod, "EEEF") && devmon->devmon_cb) {
		int what = -1;
		if(!strcmp(action, "remove"))
			what = DEVMON_DISCONNECTED;
		if(!strcmp(action, "add"))
			what = DEVMON_CONNECTED;

		if(what >= 0 && devmon->devmon_cb)
			devmon->devmon_cb(serial, what, devmon->userdata);
	}

 out:
	if(dev)
		udev_device_unref(dev);
}

struct devmon *devmon_create(struct ev_loop *loop, devmon_cb devmon_cb, void *userdata) {
	struct devmon *devmon;
	struct udev *udev;
	struct udev_monitor *mon;

	dbg1("%s()", __func__);

	udev = udev_new();
	if(udev == NULL)
		return NULL;

	mon = udev_monitor_new_from_netlink(udev, "udev");
	if(mon == NULL) {
		udev_unref(udev);
		return NULL;
	}

	devmon = w_calloc(1, sizeof(*devmon));
	devmon->devmon_cb = devmon_cb;
	devmon->userdata = userdata;
	devmon->udev = udev;
	devmon->mon = mon;
	devmon->loop = loop;
#ifdef HAVE_UDEV_MONITOR_FILTER_ADD_MATCH_SUBSYSTEM_DEVTYPE
	udev_monitor_filter_add_match_subsystem_devtype(devmon->mon, "tty", NULL);
#endif
	udev_monitor_enable_receiving(devmon->mon);
	devmon->fd = udev_monitor_get_fd(devmon->mon);

	ev_io_init(&devmon->w, mon_cb, devmon->fd, EV_READ);
	devmon->w.data = devmon;
	ev_io_start(loop, &devmon->w);

	// Invoke callback for devices that are already online
	struct devinfo *di;
	struct PGList *l = udv_get_device_list();
	int cnt = 0;

	PG_SCANLIST(l, di) {
		devmon->devmon_cb(di->serial, DEVMON_CONNECTED, devmon->userdata);
		cnt++;
	}
	udv_free_device_list(l);
	dbg0("%d device(s) found!", cnt);


	return devmon;
}

void devmon_destroy(struct devmon *devmon) {
	ev_io_stop(devmon->loop, &devmon->w);

	if(devmon->mon)
		udev_monitor_unref(devmon->mon);
	if(devmon->udev)
		udev_unref(devmon->udev);

	free(devmon);
}

#if 0
int test_wait_for_device() {
	struct udev *udev;
	struct udev_monitor *mon;
	struct udev_device *dev;

	udev = udev_new();
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "tty", NULL);
	udev_monitor_filter_add_match_tag(mon, "0403");
	
	udev_monitor_enable_receiving(mon);

	dev = udev_monitor_receive_device(mon);

	do {

		if (dev) {

			printf("Got Device\n");
			printf("   Node: %s\n", udev_device_get_devnode(dev));
			printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));
			printf("   Devtype: %s\n", udev_device_get_devtype(dev));
			printf("   Action: %s\n", udev_device_get_action(dev));
			printf("manufacturer: %s\n", udev_device_get_sysattr_value(dev,"manufacturer"));
			printf("product: %s\n", udev_device_get_sysattr_value(dev,"product"));
			printf("idVendor: %s\n", udev_device_get_sysattr_value(dev,"idVendor"));
			printf("serial: %s\n", udev_device_get_sysattr_value(dev,"serial"));
			printf("--\n\n");


			dev = udev_device_get_parent_with_subsystem_devtype(
                                                                            dev,
                                                                            "usb",
                                                                            "usb_device");
		}


	} while(dev);

	return 0;
}
#endif

void udv_print_device_list(FILE *f) {
	struct devinfo *di;
	struct PGList *l = udv_get_device_list();

	if(!l)
		return;

	fprintf(f, "Available microHam devices:\n\n");
	PG_SCANLIST(l, di) {
		fprintf(f,
			"Type:     %s\n"
			"Serial:   %s\n"
			"Device:   %s\n\n\n", di->name, di->serial, di->device);
	}
	udv_free_device_list(l);
}

const char *udv_dev_by_serial(const char *serial) {
	struct devinfo *di;
	struct PGList *l = udv_get_device_list();
	const char *res = NULL;

	PG_SCANLIST(l, di) {
		if(*serial == '*' || !strcasecmp(di->serial, serial)) {
			res = w_strdup(di->device);
			break;
		}
	}

	udv_free_device_list(l);
	return res;
}
