/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef APP_CTX_H
#define APP_CTX_H

#include <stdint.h>
#include "events.h"

typedef struct app_ctx app_ctx;
struct PGList;
struct ev_loop;
struct device;
struct conmgr;
struct device_manager;
struct cfgmgr;
struct cfgmgrj;
struct http_server;
struct restapi;
struct webui;

typedef struct eventbus eventbus_t;
typedef void (*app_ctx_event_cb_fn)(enum app_event_type type, const void *data, void *user_data);

app_ctx *app_ctx_create(void);
app_ctx *app_ctx_init(struct app_ctx *ctx, struct ev_loop *loop);
void    app_ctx_destroy(app_ctx *app_ctx);
void    app_ctx_run(app_ctx *ctx);

void    app_ctx_set_demo_mode(app_ctx *ctx, uint8_t enabled);
uint8_t app_ctx_demo_mode_enabled(app_ctx *ctx);


// FIXME: should not be needed, everything to be managed by app_ctx alone.
struct device_manager;
struct device_manager *app_ctx_get_device_manager(app_ctx *ctx);
struct device *app_ctx_get_device(app_ctx *ctx, const char *serial);

struct ev_loop *app_ctx_get_loop(app_ctx *ctx);
struct conmgr *app_ctx_get_conmgr(app_ctx *ctx);
struct PGList *app_ctx_get_device_list(app_ctx *ctx);
eventbus_t *app_ctx_get_eventbus(app_ctx *ctx);

int8_t app_ctx_add_device(struct app_ctx *ctx, const char *serial);



#endif /* APP_CTX_H */