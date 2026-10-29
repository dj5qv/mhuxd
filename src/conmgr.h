/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
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
	int ptt_rts;
	int ptt_dtr;
};

struct con_tcp_cfg {
	const char *port;
	int remote_access;
};

struct con_cfg {
	const char *serial;
	int channel;
	enum con_type type;
	// CAT channel with native CAT: CI-V transceive and auto information are not passed on
	// (JSON forward_unsolicited false).
	int no_unsolicited;
	struct con_vsp_cfg vsp;
	struct con_tcp_cfg tcp;
};

// Called by a connector that stopped working, after it has closed its ends of the router
// sockets. It is not called from *_destroy().
typedef void (*con_fail_cb)(void *user_data);

struct con_client;

// Called by a connector when an application connects, and with client NULL when it leaves.
// Not called from *_destroy(), and not for the application a failing connector drops.
typedef void (*con_client_cb)(void *user_data, const struct con_client *client);

struct connector_spec {
	struct ev_loop *loop;
	int fd_data;
	int fd_ptt;
	struct con_vsp_cfg vsp;
	struct con_tcp_cfg tcp;
	con_fail_cb on_fail;
	void *on_fail_data;
	con_client_cb on_client;
	void *on_client_data;
};

enum con_status {
	CON_STATUS_FAILED = 0,	// failed to start, or stopped working
	CON_STATUS_OK,
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
	int ptt_rts;
	int ptt_dtr;
	int remote_access;
	int no_unsolicited;
};

struct app_ctx;

typedef void (*conmgr_iter_cb)(const struct con_info *info, void *user_data);

struct conmgr *conmgr_create(void);
void conmgr_destroy(struct conmgr *conmgr);
int conmgr_create_con(struct app_ctx *ctx, struct cfg *cfg, int id);
int conmgr_create_con_cfg(struct app_ctx *app_ctx, const struct con_cfg *cfg, int id);
int conmgr_destroy_con(struct conmgr *, int id);
void conmgr_destroy_all(struct conmgr *);
void conmgr_destroy_device_cons(struct conmgr *conmgr, const struct device *dev);
void conmgr_foreach(struct conmgr *conmgr, conmgr_iter_cb cb, void *user_data);
// A connector that is not registered (it failed to start) is CON_STATUS_FAILED, too.
enum con_status conmgr_get_status(struct conmgr *conmgr, int id);
const char *conmgr_status_str(enum con_status status);
// The application on the port, NULL if there is none.
const struct con_client *conmgr_get_client(struct conmgr *conmgr, int id);
// Keep a new connector from getting this id, e.g. for a configured connector that has not been created.
void conmgr_reserve_id(struct conmgr *conmgr, int id);

#endif // CONMGR_H
