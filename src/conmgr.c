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
#include "channel.h"
#include "mhrouter.h"
#include "devmgr.h"
#include "con_vsp.h"
#include "con_tcp.h"
#include "proc_mcp.h"
#include "proc_rotator.h"

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
	int maxcon;
	int ptt_rts;
	int ptt_dtr;
	int remote_access;
};

struct conmgr {
	int id_cnt;
	struct PGList connector_list;
};

struct conmgr *conmgr_create() {
	struct conmgr *conmgr = w_calloc(1, sizeof(*conmgr));
	PG_NewList(&conmgr->connector_list);
	return conmgr;
}

void conmgr_destroy(struct conmgr *conmgr) {
	conmgr_destroy_all(conmgr);
	free(conmgr);
}

int conmgr_create_con_cfg(struct conmgr *conmgr, struct ev_loop *loop, const struct con_cfg *cfg, int id) {
	const char *serial;
	struct connector *ctr = NULL;
	int sodat[2] = { -1, -1 };
	int soptt[2] = { -1, -1 };
	struct connector_spec cspec;
	const char *type_str = "UNKNOWN";

	dbg1("%s()", __func__);

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
	if(id && id > conmgr->id_cnt)
		conmgr->id_cnt = id;

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

	ctr->dev = dmgr_get_device(serial);
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
		ctr->devname = cfg->vsp.devname ? w_strdup(cfg->vsp.devname) : NULL;
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

	// create the data sockets
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, sodat)) {
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
	mhr_add_consumer(ctr->dev->router, sodat[0], ctr->channel, serial);
	mhr_add_producer(ctr->dev->router, sodat[0], ctr->channel, serial);
	ctr->router_owns_data = 1;

	cspec.vsp = cfg->vsp;
	cspec.tcp = cfg->tcp;

	if(ctr->type == CON_VSP && (cfg->vsp.ptt_rts || cfg->vsp.ptt_dtr)) {
		// create the ptt sockets for PTT control via RTS/DTR
		ctr->ptt_channel = ch_ptt_channel(ctr->channel);

		if(ctr->ptt_channel == ctr->channel) {
			ctr->s_fd_ptt = ctr->s_fd_data;
			cspec.fd_ptt = cspec.fd_data;
		} else {
			if(ctr->ptt_channel != -1) {
				if(socketpair(AF_UNIX, SOCK_STREAM, 0, soptt)) {
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
				mhr_add_consumer(ctr->dev->router, soptt[0], ctr->ptt_channel, serial);
				mhr_add_producer(ctr->dev->router, soptt[0], ctr->ptt_channel, serial);
				ctr->router_owns_ptt = 1;
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
		ctr->instance = ctcp_create(&cspec);
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
			mhr_rem_consumer(ctr->dev->router, sodat[0], ctr->channel);
			mhr_rem_producer(ctr->dev->router, sodat[0], ctr->channel);
		}
		if(ctr->ptt_channel != -1 && ctr->router_owns_ptt) {
			mhr_rem_consumer(ctr->dev->router, soptt[0], ctr->ptt_channel);
			mhr_rem_producer(ctr->dev->router, soptt[0], ctr->ptt_channel);
		}

		fd_close(&sodat[1]);
		fd_close(&soptt[1]);
	}

	if(ctr)
	{
		if(ctr->devname)
			free(ctr->devname);
		free(ctr);
	}
	return 0;
}

int conmgr_destroy_con(struct conmgr *conmgr, int id) {
	struct connector *ctr;
	
	dbg1("%s()", __func__);

	PG_SCANLIST(&conmgr->connector_list, ctr) {
		if(ctr->id == id) {
			if(ctr->ptt_channel != -1 && ctr->ptt_channel != ctr->channel) {
				if(!ctr->router_owns_ptt)
					dbg0("connector %d ptt fd ownership mismatch, expected router owner", ctr->id);
				mhr_rem_producer(ctr->dev->router, ctr->s_fd_ptt, ctr->ptt_channel);
				mhr_rem_consumer(ctr->dev->router, ctr->s_fd_ptt, ctr->ptt_channel);
			}

			if(ctr->mcp) {
				mhr_rem_processor_cb(ctr->dev->router, mcp_cb, CH_MCP);
				mcp_destroy(ctr->mcp);
				ctr->mcp = NULL;
			}

			if(ctr->rot) {
				mhr_rem_processor_cb(ctr->dev->router, mcp_cb, CH_ROTATOR);
				rot_destroy(ctr->rot);
				ctr->rot = NULL;
			}
			
			if(!ctr->router_owns_data)
							dbg0("connector %d data fd ownership mismatch, expected router owner", ctr->id);
			mhr_rem_producer(ctr->dev->router, ctr->s_fd_data, ctr->channel);
			mhr_rem_consumer(ctr->dev->router, ctr->s_fd_data, ctr->channel);

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

			PG_Remove(&ctr->node);
			if(ctr->devname)
				free(ctr->devname);
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
		cb(&info, user_data);
	}
}

int conmgr_exists(struct conmgr *conmgr, int id) {
	if(!conmgr)
		return 0;
	struct connector *ctr;
	PG_SCANLIST(&conmgr->connector_list, ctr) {
		if(ctr->id == id)
			return 1;
	}
	return 0;
}

