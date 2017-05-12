/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef LINUX_UDEV_H
#define LINUX_UDEV_H

struct ev_loop;

enum {
	DEVMON_CONNECTED = 1,
	DEVMON_DISCONNECTED = 2,
};

//struct devmon;
typedef void (*devmon_cb)(const char *serial, int status, void *userdata);


void udv_print_device_list(FILE *f);
const char *udv_dev_by_serial(const char *serial);

struct devmon *devmon_create(struct ev_loop *loop, devmon_cb devmon_cb, void *userdata);
void devmon_destroy(struct devmon *devmon);

#endif // LINUX_UDEV_H
