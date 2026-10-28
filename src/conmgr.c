/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "conmgr.h"
#include "util.h"
#include "pglist.h"
#include "logger.h"
#include "app_ctx.h"
#include "channel.h"
#include "mhrouter.h"
#include "devmgr.h"
#include "con_vsp.h"
#include "con_tcp.h"
#include "proc_mcp.h"
#include "proc_rotator.h"
#include "mhcontrol.h"
#include "eventbus.h"

#define MOD_ID "con"

struct connector {
	struct PGNode node;
	int type;
	int id;
	int channel;
	int ptt_channel;
	/* Router-side socketpair endpoint; ownership transfers to mhrouter after mhr_add_* succeeds. */
	int s_fd_data;
	/* Router-side PTT endpoint; ownership transfers to mhrouter after mhr_add_* succeeds. */
	int s_fd_ptt;
	int router_owns_data;
	int router_owns_ptt;
	void *instance;
	struct device *dev;
	struct proc_mcp *mcp;
	struct proc_rotator *rot;
	char *devname;
	char *tag;		// router owner tag: VSP device name, "tcp:<port>"
	int maxcon;
	int ptt_rts;
	int ptt_dtr;
	int remote_access;
	int no_unsolicited;
	int failed;		// the instance stopped working, see con_failed()
	eventbus_t *ebus;
};

struct conmgr {
	int id_cnt;
	struct PGList connector_list;
};

struct conmgr *conmgr_create(void) {
	struct conmgr *conmgr = w_calloc(1, sizeof(*conmgr));
	PG_NewList(&conmgr->connector_list);
	return conmgr;
}

void conmgr_destroy(struct conmgr *conmgr) {
	conmgr_destroy_all(conmgr);
	free(conmgr);
}

// The connector is kept, so its id stays taken and deleting the port works as usual. It
// has closed its ends of the router sockets, the router finds out by itself and closes
// its ends after reading what is left, a PTT off among it.
static void con_failed(void *user_data) {
	struct connector *ctr = user_data;

	ctr->failed = 1;
	if(ctr->ebus)
		eventbus_publish(ctr->ebus, EV_CON_STATUS, &(struct ev_con_status) {
		    .connector_id = ctr->id,
		    .status = CON_STATUS_FAILED,
		});
}

// The router closes an endpoint on its own when the connector side hangs up, and the fd
// number may belong to another endpoint by now. The router keeps the tag pointer it was
// handed, so an endpoint still tagged with ours is ours: no other connector's tag can
// have the same address while ctr is alive.
static void rem_router_endpoint(struct connector *ctr, int fd, int channel) {
	if(mhr_get_producer_tag(ctr->dev->router, channel, fd) != ctr->tag) {
		dbg1("connector %d fd %d already closed by the router", ctr->id, fd);
		return;
	}
	mhr_rem_endpoint_fd(ctr->dev->router, fd, channel);
}

int conmgr_create_con_cfg(struct app_ctx *app_ctx, const struct con_cfg *cfg, int id) {
	const char *serial;
	struct connector *ctr = NULL;
	int sodat[2] = { -1, -1 };
	int soptt[2] = { -1, -1 };
	struct connector_spec cspec;
	const char *type_str = "UNKNOWN";

	dbg1("%s()", __func__);


	struct conmgr *conmgr = app_ctx_get_conmgr(app_ctx);
	struct ev_loop *loop = app_ctx_get_loop(app_ctx);

	if(!conmgr || !loop || !cfg) {
		err("%s() missing parameters", __func__);
		return 0;
	}

	if(id > 0) {
		struct connector *ctr;
		PG_SCANLIST(&conmgr->connector_list, ctr) {
			if(ctr->id == id) {
				err("can't create connector id %d, already exists!", id);
				return 0;
			}
		}
	}

	// Always "allocate" the id if it is != 0 even if the connector creation fails.
	conmgr_reserve_id(conmgr, id);

	ctr = w_calloc(1, sizeof(*ctr));
	ctr->s_fd_data = -1;
	ctr->s_fd_ptt = -1;
	ctr->channel = -1;
	ctr->ptt_channel = -1;
	ctr->type = CON_INVALID;

	memset(&cspec, 0, sizeof(cspec));
	cspec.loop = loop;
	cspec.fd_data = -1;
	cspec.fd_ptt = -1;

	// get device
	serial = cfg->serial;
	if(serial == NULL || !*serial) {
		err("can't create connector, missing parameter 'serial'!");
		goto fail;
	}

	ctr->dev =  app_ctx_get_device(app_ctx, serial);
	if(ctr->dev == NULL) {
		err("can't create connector, device %s not found!", serial);
		goto fail;
	}

	// get channel
	ctr->channel = cfg->channel;
	if(ctr->channel < 0) {
		err("can't create connector, invalid parameter for 'channel' (%d)!", ctr->channel);
		goto fail;
	}

	// get type
	ctr->type = cfg->type;
	if(ctr->type == CON_VSP)
		type_str = "VSP";
	else if(ctr->type == CON_TCP)
		type_str = "TCP";

	dbg0("%s() %s %s %s", __func__, serial, type_str, ch_channel2str(ctr->channel));

	if(ctr->type != CON_VSP && ctr->type != CON_TCP) {
		err("can't create connector, invalid parameter for 'type' (%d)", ctr->type);
		goto fail;
	}

	if(ctr->type == CON_VSP) {
		if(!vsp_devname_is_valid(cfg->vsp.devname)) {
			err("can't create connector, invalid parameter for 'devname' (%s)!",
			    cfg->vsp.devname ? cfg->vsp.devname : "");
			goto fail;
		}
		ctr->devname = w_strdup(cfg->vsp.devname);
		ctr->maxcon = cfg->vsp.maxcon;
		ctr->ptt_rts = cfg->vsp.ptt_rts;
		ctr->ptt_dtr = cfg->vsp.ptt_dtr;
		ctr->remote_access = 0;
	} else if(ctr->type == CON_TCP) {
		ctr->devname = cfg->tcp.port ? w_strdup(cfg->tcp.port) : NULL;
		ctr->maxcon = cfg->tcp.maxcon;
		ctr->ptt_rts = 0;
		ctr->ptt_dtr = 0;
		ctr->remote_access = cfg->tcp.remote_access;
	}

	if(ctr->type == CON_TCP) {
		size_t len = strlen(ctr->devname ? ctr->devname : "") + 8;
		ctr->tag = w_malloc(len);
		snprintf(ctr->tag, len, "tcp:%s", ctr->devname ? ctr->devname : "");
	} else {
		ctr->tag = w_strdup(ctr->devname);
	}

	// create the data sockets
	// SOCK_CLOEXEC: keep the sockets out of child processes (rigctld), they would keep
	// the router endpoint alive after the connector has been destroyed.
	if(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sodat)) {
		err_e(errno, "could not create data socket for %s!", type_str);
		goto fail;
	}
	if (fcntl(sodat[0], F_SETFL, O_NONBLOCK) < 0) {
		err_e(errno, "failed to set NONBLOCK on data socket");
		goto fail;
	}
	if (fcntl(sodat[1], F_SETFL, O_NONBLOCK) < 0) {
		err_e(errno, "failed to set NONBLOCK on data socket");
		goto fail;
	}
	ctr->s_fd_data = sodat[0];
	cspec.fd_data = sodat[1];
	mhr_add_endpoint_fd(ctr->dev->router, sodat[0], ctr->channel, ctr->tag);
	ctr->router_owns_data = 1;
	ctr->no_unsolicited = cfg->no_unsolicited;
	if(ctr->no_unsolicited)
		mhr_set_consumer_flags(ctr->dev->router, sodat[0], ctr->channel, MHR_CNS_NO_UNSOLICITED);

	cspec.vsp = cfg->vsp;
	cspec.tcp = cfg->tcp;
	cspec.on_fail = con_failed;
	cspec.on_fail_data = ctr;
	ctr->ebus = app_ctx_get_eventbus(app_ctx);

	if(ctr->type == CON_VSP && (cfg->vsp.ptt_rts || cfg->vsp.ptt_dtr)) {
		// create the ptt sockets for PTT control via RTS/DTR
		ctr->ptt_channel = ch_ptt_channel(ctr->channel);

		if(ctr->ptt_channel == ctr->channel) {
			ctr->s_fd_ptt = ctr->s_fd_data;
			cspec.fd_ptt = cspec.fd_data;
		} else {
			if(ctr->ptt_channel != -1) {
				if(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, soptt)) {
					err_e(errno, "could not create ptt socket for VSP!");
					goto fail;
				}
				if (fcntl(soptt[0], F_SETFL, O_NONBLOCK) < 0) {
					err_e(errno, "failed to set NONBLOCK on ptt socket");
					goto fail;
				}
				if (fcntl(soptt[1], F_SETFL, O_NONBLOCK) < 0) {
					err_e(errno, "failed to set NONBLOCK on ptt socket");
					goto fail;
				}
				ctr->s_fd_ptt = soptt[0];
				cspec.fd_ptt = soptt[1];
				mhr_add_endpoint_fd(ctr->dev->router, soptt[0], ctr->ptt_channel, ctr->tag);
				ctr->router_owns_ptt = 1;
			} else {
				err("Could not determine ptt channel for channel %s", ch_channel2str_new(ctr->channel, mhc_get_mhinfo(ctr->dev->ctl)));
				goto fail;
			}
		}
	}

	// create the connector
	dbg1("creating %s connector", type_str);

	switch(ctr->type) {
	case CON_VSP:
		ctr->instance = vsp_create(&cspec);
		break;
	case CON_TCP:
		ctr->instance = ctcp_create(&cspec, ch_is_ptt_channel(ctr->channel));
		break;
	default:
		err("create connector, invalid type %d!", ctr->type);
		goto fail;
	}

	if(ctr->instance) {
		if(id > 0) {
			ctr->id = id;
		} else {
			ctr->id = ++conmgr->id_cnt;
		}

		if(ctr->channel == CH_MCP) {
			ctr->mcp = mcp_create(ctr->dev->ctl, CH_MCP);
			mhr_add_processor_cb(ctr->dev->router, mcp_cb, CH_MCP, ctr->mcp);
		}

		if(ctr->channel == CH_ROTATOR) {
			ctr->rot = rot_create(ctr->dev->ctl, CH_ROTATOR);
			mhr_add_processor_cb(ctr->dev->router, rot_cb, CH_ROTATOR, ctr->rot);
		}

		PG_AddTail(&conmgr->connector_list, &ctr->node);
	} else {
		err("failed to create connector!");
		goto fail;
	}

	return ctr->id;

 fail:
	if(ctr->dev) {
		if(ctr->router_owns_data) {
			mhr_rem_endpoint_fd(ctr->dev->router, sodat[0], ctr->channel);
		}
		if(ctr->ptt_channel != -1 && ctr->router_owns_ptt) {
			mhr_rem_endpoint_fd(ctr->dev->router, soptt[0], ctr->ptt_channel);
		}

		fd_close(&sodat[1]);
		fd_close(&soptt[1]);
	}

	if(ctr)
	{
		if(ctr->devname)
			free(ctr->devname);
		free(ctr->tag);
		free(ctr);
	}
	return 0;
}

int conmgr_destroy_con(struct conmgr *conmgr, int id) {
	struct connector *ctr;
	
	dbg1("%s()", __func__);

	if(!conmgr) {
		warn("%s() conmgr is NULL", __func__);
		return -1;
	}

	PG_SCANLIST(&conmgr->connector_list, ctr) {
		if(ctr->id == id) {
			// The instance first, while the router ends are still open: its PTT off for
			// clients that keyed goes to the router instead of a closed socket. The
			// router releases what the removed endpoints still hold anyway.
			switch(ctr->type) {
			case CON_VSP:
				vsp_destroy(ctr->instance);
				break;
			case CON_TCP:
				ctcp_destroy(ctr->instance);
				break;
			default:
				break;
			}

			if(ctr->ptt_channel != -1 && ctr->ptt_channel != ctr->channel) {
				if(!ctr->router_owns_ptt)
					dbg0("connector %d ptt fd ownership mismatch, expected router owner", ctr->id);
				rem_router_endpoint(ctr, ctr->s_fd_ptt, ctr->ptt_channel);
			}

			if(ctr->mcp) {
				mhr_rem_processor_cb(ctr->dev->router, mcp_cb, CH_MCP);
				mcp_destroy(ctr->mcp);
				ctr->mcp = NULL;
			}

			if(ctr->rot) {
				mhr_rem_processor_cb(ctr->dev->router, rot_cb, CH_ROTATOR);
				rot_destroy(ctr->rot);
				ctr->rot = NULL;
			}
			
			if(!ctr->router_owns_data)
							dbg0("connector %d data fd ownership mismatch, expected router owner", ctr->id);
			rem_router_endpoint(ctr, ctr->s_fd_data, ctr->channel);

			PG_Remove(&ctr->node);
			if(ctr->devname)
				free(ctr->devname);
			free(ctr->tag);
			free(ctr);
			return 0;
		}
	}
	return -1;
}

void conmgr_destroy_all(struct conmgr *conmgr) {
	struct connector *ctr;
	while((ctr = (void*)PG_FIRSTENTRY(&conmgr->connector_list))) {
		conmgr_destroy_con(conmgr, ctr->id);
	}
}

void conmgr_destroy_device_cons(struct conmgr *conmgr, const struct device *dev) {
	struct connector *ctr;
	if(!conmgr)
		return;
	while(1) {
		struct connector *match = NULL;
		PG_SCANLIST(&conmgr->connector_list, ctr) {
			if(ctr->dev == dev) {
				match = ctr;
				break;
			}
		}
		if(!match)
			break;
		conmgr_destroy_con(conmgr, match->id);
	}
}

void conmgr_foreach(struct conmgr *conmgr, conmgr_iter_cb cb, void *user_data) {
	if(!conmgr || !cb)
		return;
	struct connector *ctr;
	PG_SCANLIST(&conmgr->connector_list, ctr) {
		struct con_info info;
		info.id = ctr->id;
		info.serial = ctr->dev ? ctr->dev->serial : NULL;
		info.channel = ctr->channel;
		info.type = ctr->type;
		info.devname = ctr->devname;
		info.maxcon = ctr->maxcon;
		info.ptt_rts = ctr->ptt_rts;
		info.ptt_dtr = ctr->ptt_dtr;
		info.remote_access = ctr->remote_access;
		info.no_unsolicited = ctr->no_unsolicited;
		cb(&info, user_data);
	}
}

void conmgr_reserve_id(struct conmgr *conmgr, int id) {
	if(conmgr && id > conmgr->id_cnt)
		conmgr->id_cnt = id;
}

enum con_status conmgr_get_status(struct conmgr *conmgr, int id) {
	if(!conmgr)
		return CON_STATUS_FAILED;
	struct connector *ctr;
	PG_SCANLIST(&conmgr->connector_list, ctr) {
		if(ctr->id == id)
			return ctr->failed ? CON_STATUS_FAILED : CON_STATUS_OK;
	}
	return CON_STATUS_FAILED;
}

const char *conmgr_status_str(enum con_status status) {
	return status == CON_STATUS_OK ? "ok" : "failed";
}

