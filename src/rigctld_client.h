/*
 *  mhuxd - microHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2024  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef RIGCTLD_CLIENT_H
#define RIGCTLD_CLIENT_H

struct rigctld_client;
struct ev_loop;
struct mh_control;

/*
 * Create a rigctld client that connects to a rigctld server at host:port,
 * periodically queries frequency and mode, and sends updates to the keyer.
 * rig_idx: 0 = Radio 1 (R1), 1 = Radio 2 (R2)
 */
struct rigctld_client *rgc_create(struct ev_loop *loop, struct mh_control *ctl,
				  const char *host, int port, int rig_idx);
void rgc_destroy(struct rigctld_client *rgc);

#endif /* RIGCTLD_CLIENT_H */
