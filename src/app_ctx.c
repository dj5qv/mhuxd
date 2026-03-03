/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <errno.h>
#include <ev.h>
#include "app_ctx.h"
#include "logger.h"
#include "clearsilver/util/neo_err.h"
#include "config.h"
#include "util.h"
#include "opts.h"
#include "devmgr.h"
#include "conmgr.h"
#include "cfgmgr.h"
#include "cfgmgrj.h"
#include "http_server.h"
#include "webui.h"
#include "restapi.h"
#include "eventbus.h"
#include "events.h"

#define MOD_ID "ctx"

typedef struct app_ctx {
    struct ev_loop *loop;
    eventbus_t *ebus;
    struct device_manager *dmgr;
    struct conmgr *conmgr;
    struct cfgmgr *cfgmgr;
    struct cfgmgrj *cfgmgrj;
    struct http_server *hs;
    struct restapi *restapi;
    struct webui *webui;
    struct http_handler *handler_redir[1];

    struct ev_signal w_sigint, w_sigterm, w_sighup;

} app_ctx;

static int signum = 0;
static void sigint_cb(struct ev_loop *loop, struct ev_signal *w, int revents)
{
	(void)w; (void)revents;
	signum = w->signum;
	ev_break(loop, EVBREAK_ALL);
}
static void sighup_cb(struct ev_loop *loop, struct ev_signal *w, int revents) {
        info("*** SIGHUP received-> closing log file");
        log_reopen(); 
}

static int cb_redirect_home(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	dbg1("redirect / to /svelte/index.html");
	hs_add_rsp_header(hcon, "Location", "/svelte/index.html");
	hs_send_response(hcon, 301, "text/html", NULL, 0, NULL, 0);
	return 0;
}

app_ctx *app_ctx_create() {
    struct app_ctx *ctx = w_calloc(1, sizeof(app_ctx));
    ctx->ebus = eventbus_create();
    return ctx;
}

app_ctx *app_ctx_init(struct app_ctx *ctx, struct ev_loop *loop) {
    ctx->loop = loop;

    ctx->dmgr = dmgr_create(loop, ctx->ebus);
    ctx->conmgr = conmgr_create();
    ctx->cfgmgr = cfgmgr_create(ctx);

    if(!ctx->cfgmgr) {
		fatal("(mhuxd) Could not create configuration manager, exiting!");
		exit(-1);
	}

    ctx->cfgmgrj = cfgmgrj_create(ctx);
	if(!ctx->cfgmgrj) {
		fatal("(mhuxd) Could not create json manager, exiting!");
		exit(-1);
	}

	ctx->hs = hs_start(loop, webui_host_port);
	if(!ctx->hs) {
		fatal("(mhuxd) Could not start webserver, exiting!");
		exit(-1);
	}

    ctx->restapi = restapi_create(ctx, ctx->hs, ctx->cfgmgrj);
	if(!ctx->restapi) {
		fatal("(mhuxd) Could not start REST API, exiting!");
		hs_stop(ctx->hs);
		exit(-1);
	}

	ctx->webui = webui_create(ctx->hs, ctx->cfgmgr);
	if(!ctx->webui) {
		fatal("(mhuxd) Could not start webui, exiting!");
		restapi_destroy(ctx->restapi);
		hs_stop(ctx->hs);
		exit(-1);
	}

	static const char static_path[] = WEBUIDIR "/static";
	static const char svelte_path[] = WEBUIDIR "/svelte";
	hs_add_directory_map(ctx->hs, "/static/", static_path);
	hs_add_directory_map(ctx->hs, "/svelte/", svelte_path);
	ctx->handler_redir[0] = hs_register_handler(ctx->hs, "/", cb_redirect_home, ctx->webui);

	ev_signal_init (&ctx->w_sigint, sigint_cb, SIGINT);
	ev_signal_init (&ctx->w_sigterm, sigint_cb, SIGTERM);
	ev_signal_init (&ctx->w_sighup, sighup_cb, SIGHUP);

	ev_signal_start (loop, &ctx->w_sigint);
	ev_signal_start (loop, &ctx->w_sigterm);
	ev_signal_start (loop, &ctx->w_sighup);

/*
    if(cfgmgr_init(cfgmgr))
        err("(main) error initializing config manager!");
*/
    int rc_j = cfgmgrj_load_cfg(ctx->cfgmgrj);
	if(rc_j == 0)
		info("(main) json config loaded successfully.");

	if(rc_j == -2) {
        info("(main) json config not found, attempting migration from legacy HDF...");
        if(cfgmgr_init(ctx->cfgmgr) == 0) {
            cfgmgrj_sync_from_conmgr(ctx->cfgmgrj);
            cfgmgrj_save_cfg(ctx->cfgmgrj);
            info("(main) hdf to json migration complete.");
        } else {
            err("(main) legacy config migration failed.");
        }
    } else if(rc_j < 0) {
        err("(main) error initializing json config manager!");
    }

    dmgr_enable_monitor(ctx->dmgr);

    
	if(demo_mode) { // FIXME: global referenced.
		dmgr_add_device(ctx->dmgr, "CK_DEMO_CK_1");
		dmgr_add_device(ctx->dmgr, "DK_DEMO_DK_1");
		dmgr_add_device(ctx->dmgr, "D2_DEMO_DK2_1");
		dmgr_add_device(ctx->dmgr, "MK_DEMO_MK_1");
		dmgr_add_device(ctx->dmgr, "M2_DEMO_MK2_1");
		dmgr_add_device(ctx->dmgr, "M3_DEMO_MK3_1");
		dmgr_add_device(ctx->dmgr, "2R_DEMO_MK2R_1");
		dmgr_add_device(ctx->dmgr, "2P_DEMO_MK2Rp_1");
		dmgr_add_device(ctx->dmgr, "UR_DEMO_U2R_1");
		dmgr_add_device(ctx->dmgr, "SM_DEMO_SM_1");
		dmgr_add_device(ctx->dmgr, "SD_DEMO_SMD_1");
	}


    return ctx;
}

void app_ctx_destroy(app_ctx *ctx) {
    if(!ctx) return;

    if(ctx->dmgr)
        dmgr_disable_monitor(ctx->dmgr);

    // shutdown restapi and have it unregister all callbacks.
    // Don't want it to mess around with lower level modules now.
    if(ctx->restapi)
        restapi_shutdown(ctx->restapi);

    if(ctx->handler_redir[0])
   	    hs_unregister_handler(ctx->hs, ctx->handler_redir[0]);

    if(ctx->webui)
        webui_destroy(ctx->webui);

    if(ctx->hs)
        hs_stop(ctx->hs);

    cfgmgr_save_cfg(ctx->cfgmgr);
    cfgmgrj_save_cfg(ctx->cfgmgrj);

    if(ctx->cfgmgr)
        cfgmgr_destroy(ctx->cfgmgr);
    if(ctx->cfgmgrj)
        cfgmgrj_destroy(ctx->cfgmgrj);
    if(ctx->conmgr)
        conmgr_destroy(ctx->conmgr);
    if(ctx->restapi)
        restapi_destroy(ctx->restapi);
    if(ctx->dmgr)
        dmgr_destroy(ctx->dmgr);
    if(ctx->ebus) 
        eventbus_destroy(ctx->ebus);

    free(ctx);
} 

void app_ctx_run(app_ctx *ctx) {
    if(!ctx) return;

    ev_run(ctx->loop, 0);

    if(signum) 
        info("*** %s received!", strsignal(signum));
}

struct device_manager *app_ctx_get_device_manager(app_ctx *ctx) {
    return ctx->dmgr;
}

// FIXME: needed?
struct device *app_ctx_get_device(app_ctx *ctx, const char *serial) {
    return ctx->dmgr ? dmgr_get_device(ctx->dmgr, serial) : NULL;
}

struct ev_loop *app_ctx_get_loop(app_ctx *ctx) {
    return ctx ? ctx->loop : NULL;
}

struct conmgr *app_ctx_get_conmgr(app_ctx *ctx) {
    return ctx ? ctx->conmgr : NULL;
}

eventbus_t *app_ctx_get_eventbus(app_ctx *ctx) {
    return ctx ? ctx->ebus : NULL;
}

int8_t app_ctx_add_device(struct app_ctx *ctx, const char *serial) {
    struct device *dev = dmgr_add_device(ctx->dmgr, serial);
    return dev ? 0 : -1;
}

struct PGList *app_ctx_get_device_list(app_ctx *ctx) {
    return ctx && ctx->dmgr ? dmgr_get_device_list(ctx->dmgr) : NULL;
}

