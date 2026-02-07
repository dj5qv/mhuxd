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
struct cfg;

enum con_type {
	CON_INVALID = 0,
	CON_VSP,
	CON_TCP,
	CON_UNIX
};

struct con_vsp_cfg {
	const char *devname;
	int maxcon;
	int ptt_rts;
	int ptt_dtr;
};

struct con_tcp_cfg {
	const char *port;
	int maxcon;
	int remote_access;
};

struct con_cfg {
	const char *serial;
	int channel;
	enum con_type type;
	struct con_vsp_cfg vsp;
	struct con_tcp_cfg tcp;
};

struct connector_spec {
	struct ev_loop *loop;
	int fd_data;
	int fd_ptt;
	struct con_vsp_cfg vsp;
	struct con_tcp_cfg tcp;
};


struct conmgr;
struct device;
struct cli_manager;

struct con_info {
	int id;
	const char *serial;
	int channel;
	enum con_type type;
	const char *devname;
	int maxcon;
	int ptt_rts;
	int ptt_dtr;
	int remote_access;
};

typedef void (*conmgr_iter_cb)(const struct con_info *info, void *user_data);

struct conmgr *conmgr_create();
void conmgr_destroy(struct conmgr *conmgr);
int conmgr_create_con(struct conmgr *conmgr, struct ev_loop *loop, struct cfg *cfg, int id);
int conmgr_create_con_cfg(struct conmgr *conmgr, struct ev_loop *loop, const struct con_cfg *cfg, int id);
int conmgr_destroy_con(struct conmgr *, int id);
void conmgr_destroy_all(struct conmgr *);
void conmgr_foreach(struct conmgr *conmgr, conmgr_iter_cb cb, void *user_data);

#endif // CONMGR_H
