/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef LINUX_UDEV_H
#define LINUX_UDEV_H

struct PGList *udv_get_device_list();
void udv_print_device_list(FILE *f);
void udv_free_device_list(struct PGList *l);
const char *udv_dev_by_serial(const char *serial);

#endif // LINUX_UDEV_H
