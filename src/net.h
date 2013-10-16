/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef NET_H
#define NET_H

struct netlsnr;

struct netlsnr *net_create_listener(const char *host_port_str);
void net_destroy_lsnr(struct netlsnr *lsnr);
int net_listener_get_fd(struct netlsnr *lsnr);
int net_accept(int fd);

#endif /* NET_H */
