/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2017  Matthias Moeller, DJ5QV
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
#include "cfgnod.h"
#include "proc_mcp.h"
#include "proc_rotator.h"

#define MOD_ID "con"

enum {
	CON_INVALID,
	CON_VSP,
	CON_TCP,
	CON_UNIX
};

struct connector {
	struct PGNode node;
	struct connector_spec cspec;
	int type;
	int id;
	int channel;
	int ptt_channel;
	int s_fd_data;
	int s_fd_ptt;
	void *instance;
	struct device *dev;
	struct proc_mcp *mcp;
	struct proc_rotator *rot;
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

int conmgr_create_con(struct conmgr *conmgr, struct ev_loop *loop, struct cfg *cfg, int id) {
	const char *type_str, *channel_str, *serial;
	struct connector *ctr = NULL;
        int sodat[2] = { -1, -1 };
        int soptt[2] = { -1, -1 };

	dbg1("%s()", __func__);

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
	ctr->cspec.fd_data = -1;
	ctr->cspec.fd_ptt = -1;
	ctr->channel = -1;
	ctr->ptt_channel = -1;
	ctr->type = CON_INVALID;
	ctr->cspec.loop = loop;
	ctr->cspec.cfg = cfg_copy(cfg);

	// get device
	serial = cfg_get_val(cfg, "serial", NULL);
	if(serial == NULL) {
		err("can't create connector, missing parameter 'serial'!");
		goto fail;
	}

	ctr->dev = dmgr_get_device(serial);
	if(ctr->dev == NULL) {
		err("can't create connector, device %s not found!", serial);
		goto fail;
	}


	// get channel
	channel_str = cfg_get_val(cfg, "channel", NULL);

	if(channel_str == NULL) {
		err("can't create connector, missing parameter 'channel'!");
		goto fail;
	}

	ctr->channel = ch_str2channel(channel_str);

	if(ctr->channel == -1) {
		err("can't create connector, invalid parameter for 'channel' (%s)!", channel_str);
		goto fail;
	}

	// get type
	type_str = cfg_get_val(cfg, "type", NULL);

	if(type_str == NULL) {
		err("can't create connector, missing parameter 'type'!");
		goto fail;
	}

	dbg0("%s() %s %s %s", __func__, serial, type_str, channel_str);

	if(!strcasecmp(type_str, "VSP"))
		ctr->type = CON_VSP;
	if(!strcasecmp(type_str, "TCP"))
		ctr->type = CON_TCP;


	if(ctr->type == CON_INVALID) {
		err("can't create connector, invalid parameter for 'type' (%s)", type_str);
		goto fail;
	}

	// create the data sockets
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, sodat)) {
		err_e(errno, "could not create data socket for VSP!");
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
	ctr->cspec.fd_data = sodat[1];
	mhr_add_consumer(ctr->dev->router, sodat[0], ctr->channel);
	mhr_add_producer(ctr->dev->router, sodat[0], ctr->channel);


	if(ctr->type == CON_VSP && (cfg_get_int_val(cfg, "ptt_rts", 0) || cfg_get_int_val(cfg, "ptt_dtr", 0))) {
		// create the ptt sockets for PTT control via RTS/DTR
		ctr->ptt_channel = ch_ptt_channel(ctr->channel);

		if(ctr->ptt_channel == ctr->channel) {
			ctr->s_fd_ptt = ctr->s_fd_data;
			ctr->cspec.fd_ptt = ctr->cspec.fd_data;
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
				ctr->cspec.fd_ptt = soptt[1];
				mhr_add_consumer(ctr->dev->router, soptt[0], ctr->ptt_channel);
				mhr_add_producer(ctr->dev->router, soptt[0], ctr->ptt_channel);
			}
		}
	}

	// create the connector
	dbg1("creating %s connector", type_str);

	switch(ctr->type) {
	case CON_VSP:
		ctr->instance = vsp_create(&ctr->cspec);
		break;
	case CON_TCP:
		ctr->instance = ctcp_create(&ctr->cspec);
		break;
	default:
		err("create connector, invalid type %d!", ctr->type);
		goto fail;

		break;
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

		// atl_set_value_from_int(ctr->cspec.cfg, "ID", ctr->id);
		PG_AddTail(&conmgr->connector_list, &ctr->node);
	} else {
		err("failed to create connector!");
		goto fail;
	}

	return ctr->id;

 fail:
	if(ctr->dev) {
		mhr_rem_consumer(ctr->dev->router, sodat[0], ctr->channel);
		mhr_rem_producer(ctr->dev->router, sodat[0], ctr->channel);
		if(ctr->ptt_channel != -1) {
			mhr_rem_consumer(ctr->dev->router, soptt[0], ctr->ptt_channel);
			mhr_rem_producer(ctr->dev->router, soptt[0], ctr->ptt_channel);
		}


		if(sodat[0] != -1) close(sodat[0]);
		if(sodat[1] != -1) close(sodat[1]);
		if(soptt[0] != -1) close(soptt[0]);
		if(soptt[1] != -1) close(soptt[1]);
	}

	if(ctr) {
		cfg_destroy(ctr->cspec.cfg);
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
				mhr_rem_producer(ctr->dev->router, ctr->s_fd_ptt, ctr->ptt_channel);
				mhr_rem_consumer(ctr->dev->router, ctr->s_fd_ptt, ctr->ptt_channel);
				close(ctr->s_fd_ptt);
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
			
			mhr_rem_producer(ctr->dev->router, ctr->s_fd_data, ctr->channel);
			mhr_rem_consumer(ctr->dev->router, ctr->s_fd_data, ctr->channel);

			if(ctr->s_fd_data != -1)
				close(ctr->s_fd_data);

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
			if(ctr->cspec.cfg)
				cfg_destroy(ctr->cspec.cfg);
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

