/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <time.h>
#include "restapi.h"
#include "http_server.h"
#include "util.h"
#include "version.h"

#define MOD_ID "restapi"

struct restapi {
	struct http_server *hs;
	struct http_handler *health_handler;
	time_t start_time;
};

static int cb_health(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query; (void)body; (void)body_len;
	struct restapi *api = data;
	char buf[512];
	time_t now = time(NULL);
	long uptime = (now >= api->start_time) ? (long)(now - api->start_time) : 0;

	int len = snprintf(
		buf,
		sizeof(buf),
		"{\"status\":\"ok\",\"version\":\"%s\",\"build\":{\"name\":\"%s\",\"commit\":null},\"uptimeSec\":%ld}",
		_package_version,
		_package,
		uptime
	);

	if(len < 0)
		len = 0;
	if((size_t)len >= sizeof(buf))
		len = sizeof(buf) - 1;

	hs_add_rsp_header(hcon, "Cache-Control", "no-store");
	hs_send_response(hcon, 200, "application/json", buf, (size_t)len, NULL, 0);
	return 0;
}

struct restapi *restapi_create(struct http_server *hs) {
	if(!hs)
		return NULL;

	struct restapi *api = w_calloc(1, sizeof(*api));
	api->hs = hs;
	api->start_time = time(NULL);
	api->health_handler = hs_register_handler(hs, "/api/v1/health", cb_health, api);
	if(!api->health_handler) {
		free(api);
		return NULL;
	}

	return api;
}

void restapi_destroy(struct restapi *api) {
	if(!api)
		return;
	if(api->health_handler)
		hs_unregister_handler(api->hs, api->health_handler);
	free(api);
}