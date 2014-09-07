/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CONMGR_H
#define CONMGR_H

struct ev_loop;
struct connector;
struct cfg *cfg;

struct connector_spec {
	struct ev_loop *loop;
	struct cfg *cfg;
	int fd_data;
	int fd_ptt;
};


struct conmgr;
struct device;
struct cli_manager;

struct conmgr *conmgr_create();
void conmgr_destroy(struct conmgr *conmgr);
int conmgr_create_con(struct conmgr *conmgr, struct ev_loop *loop, struct cfg *cfg, int id);
int conmgr_destroy_con(struct conmgr *, int id);
void conmgr_destroy_all(struct conmgr *);

#endif // CONMGR_H
