/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef RIGCTLD_CLIENT_H
#define RIGCTLD_CLIENT_H

#include <stdint.h>

struct ev_loop;
struct mh_control;

struct rigctld_client;

struct rigctld_client_cfg {
	const char *serial;
	uint8_t radio;
	const char *backend;
	const char *host;
	int port;
	int connect_timeout_ms;
	int io_timeout_ms;
	int poll_ms;
	int enabled;
};

struct rigctld_client *rigctld_client_create(struct ev_loop *loop, struct mh_control *ctl,
					     const struct rigctld_client_cfg *cfg);
void rigctld_client_destroy(struct rigctld_client *client);

#endif // RIGCTLD_CLIENT_H
