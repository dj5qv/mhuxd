
/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <errno.h>
#include <jansson.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "pglist.h"
#include "restapi.h"
#include "app_ctx.h"
#include "http_server.h"
#include "ws.h"
#include "mhinfo.h"
#include "mhcontrol.h"
#include "device.h"
#include "cfgmgrj.h"
#include "util.h"
#include "version.h"
#include "logger.h"
#include "eventbus.h"

#define MOD_ID "restapi"

/* Number of entries in routes[], see restapi_create(). */
#define NUM_ROUTES 10

struct restapi {
	struct app_ctx *ctx;
	struct http_server *hs;
	struct cfgmgrj *cfgmgrj;
	struct http_handler *handlers[NUM_ROUTES];
	eventbus_sub_t *keyer_state_sub;
	eventbus_sub_t *keyer_mode_sub;
	struct PGList ws_subscribers;
	json_t *rigtypes;
	json_t *devicetypes;
	json_t *displayoptions;
	time_t start_time;
};

struct ws_subscriber {
	struct PGNode node;
	struct http_connection *hcon;
	struct restapi *api;
};

struct mh_flag_name {
	uint32_t flag;
	const char *name;
};

static const struct mh_flag_name mh_flag_names[] = {
	{ MHF_HAS_R1, "HAS_R1" },
	{ MHF_HAS_R2, "HAS_R2" },
	{ MHF_HAS_R1_RADIO_SUPPORT, "HAS_R1_RADIO_SUPPORT" },
	{ MHF_HAS_R2_RADIO_SUPPORT, "HAS_R2_RADIO_SUPPORT" },
	{ MHF_HAS_AUX, "HAS_AUX" },
	{ MHF_HAS_WINKEY, "HAS_WINKEY" },
	{ MHF_HAS_FSK1, "HAS_FSK1" },
	{ MHF_HAS_FSK2, "HAS_FSK2" },
	{ MHF_HAS_FRBASE, "HAS_FRBASE" },
	{ MHF_HAS_FRBASE_CW, "HAS_FRBASE_CW" },
	{ MHF_HAS_FRBASE_DIGITAL, "HAS_FRBASE_DIGITAL" },
	{ MHF_HAS_FRBASE_VOICE, "HAS_FRBASE_VOICE" },
	{ MHF_HAS_LNA_PA_PTT, "HAS_LNA_PA_PTT" },
	{ MHF_HAS_LNA_PA_PTT_TAIL, "HAS_LNA_PA_PTT_TAIL" },
	{ MHF_HAS_SOUNDCARD_PTT, "HAS_SOUNDCARD_PTT" },
	{ MHF_HAS_CW_IN_VOICE, "HAS_CW_IN_VOICE" },
	{ MHF_HAS_AUDIO_SWITCHING, "HAS_AUDIO_SWITCHING" },
	{ MHF_HAS_DISPLAY, "HAS_DISPLAY" },
	{ MHF_HAS_FOLLOW_TX_MODE, "HAS_FOLLOW_TX_MODE" },
	{ MHF_HAS_PTT_SETTINGS, "HAS_PTT_SETTINGS" },
	{ MHF_HAS_KEYER_MODE, "HAS_KEYER_MODE" },
	{ MHF_HAS_FLAGS_CHANNEL, "HAS_FLAGS_CHANNEL" },
	{ MHF_HAS_MCP_SUPPORT, "HAS_MCP_SUPPORT" },
	{ MHF_HAS_ROTATOR_SUPPORT, "HAS_ROTATOR_SUPPORT" },
	{ MHF_HAS_SM_COMMANDS, "HAS_SM_COMMANDS" },
	{ MHF_HAS_PFSK, "HAS_PFSK" },
	{ MHF_HAS_PCW, "HAS_PCW" },
	{ MHF_MHUXD_SUPPORTED, "MHUXD_SUPPORTED" }
};

/* Response helpers. They return 0 so that handlers can end with "return send_...()". */

static int send_empty(struct http_connection *hcon, uint16_t code) {
	hs_send_response(hcon, code, "application/json", "{}", 2, NULL, 0);
	return 0;
}

/* Send root as the response body and release it. A NULL root (allocation failure) sends 500. */
static int send_json(struct http_connection *hcon, uint16_t code, json_t *root) {
	char *payload = json_dumps(root, JSON_COMPACT);
	json_decref(root);
	if(!payload)
		return send_empty(hcon, 500);
	hs_add_rsp_header(hcon, "Cache-Control", "no-store");
	hs_send_response(hcon, code, "application/json", payload, strlen(payload), NULL, 0);
	free(payload);
	return 0;
}

static int send_json_error(struct http_connection *hcon, uint16_t code, const char *message) {
	json_t *rsp = json_object();
	json_object_set_new(rsp, "error", json_string(message));
	return send_json(hcon, code, rsp);
}

/* Parse buf as a JSON object. NULL if it is empty, malformed or not an object. */
static json_t *load_json_object(const char *buf, size_t len) {
	if(!buf || !len)
		return NULL;
	json_t *root = json_loadb(buf, len, 0, NULL);
	if(!json_is_object(root)) {
		json_decref(root);
		return NULL;
	}
	return root;
}

static void attach_display_options(json_t *device, json_t *displayoptions, uint16_t type) {
	json_t *map = json_object_get(displayoptions, "deviceTypeMap");
	if(!json_is_object(map))
		return;

	char type_key[16];
	snprintf(type_key, sizeof(type_key), "%u", (unsigned int)type);
	json_t *set_name = json_object_get(map, type_key);
	if(!json_is_string(set_name))
		return;

	json_t *sets = json_object_get(displayoptions, "displayOptionSets");
	json_t *set = json_object_get(sets, json_string_value(set_name));
	if(!json_is_object(set))
		return;

	json_t *bg = json_object_get(set, "displaybackground");
	if(json_is_array(bg))
		json_object_set(device, "displaybackground", bg);

	json_t *ev = json_object_get(set, "displayevent");
	if(json_is_array(ev))
		json_object_set(device, "displayevent", ev);
}

static json_t *build_devicetypes_array(json_t *displayoptions) {
	json_t *devicetypes = json_array();
	if(!devicetypes)
		return NULL;

	for(int i = 0; i < mh_info_map_size; i++) {
		const struct mh_info_map *info = &mh_info_map[i];
		json_t *device = json_object();
		json_t *flags = json_array();
		if(!device || !flags) {
			json_decref(device);
			json_decref(flags);
			json_decref(devicetypes);
			return NULL;
		}

		if(json_object_set_new(device, "type", json_integer((json_int_t)info->type)) != 0 ||
		   json_object_set_new(device, "name", json_string(info->name)) != 0 ||
		   json_object_set_new(device, "flags", flags) != 0) {
			json_decref(device);
			json_decref(devicetypes);
			return NULL;
		}

		attach_display_options(device, displayoptions, info->type);

		for(size_t f = 0; f < ARRAY_SIZE(mh_flag_names); f++) {
			if(info->flags & mh_flag_names[f].flag) {
				if(json_array_append_new(flags, json_string(mh_flag_names[f].name)) != 0) {
					json_decref(device);
					json_decref(devicetypes);
					return NULL;
				}
			}
		}

		if(json_array_append_new(devicetypes, device) != 0) {
			json_decref(devicetypes);
			return NULL;
		}
	}

	return devicetypes;
}

static int cb_metadata(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	struct restapi *api = data;

	json_t *root = json_object();
	json_object_set(root, "rigtypes", api->rigtypes);
	json_object_set(root, "devicetypes", api->devicetypes);
	return send_json(hcon, 200, root);
}

static void broadcast_event(struct restapi *api, const json_t *event) {
	char *dump = json_dumps(event, JSON_COMPACT);
	if(!dump) return;

	struct ws_subscriber *wsub;
	PG_SCANLIST(&api->ws_subscribers, wsub) {
		hs_ws_send_text(wsub->hcon, dump, strlen(dump));
	}

	free(dump);
}

static void ev_keyer_state_cb(enum app_event_type type, const void *data, void *user_data) {
	struct restapi *api = user_data;
	const struct ev_keyer_state *ev = data;
	dbg0("%s() event received: type=%d serial=%s state=%d", __func__, type, ev ? ev->serial : "NULL", ev ? ev->state : -1);
	if(type != EV_KEYER_STATE || !ev)
		return;
	json_t *event = json_object();
	json_object_set_new(event, "type", json_string("status"));
	json_object_set_new(event, "serial", json_string(ev->serial));
	json_object_set_new(event, "status", json_string(mhc_state_str(ev->state)));
	broadcast_event(api, event);
	json_decref(event);
}

static void ev_keyer_mode_cb(enum app_event_type type, const void *data, void *user_data) {
	struct restapi *api = user_data;
	const struct ev_keyer_mode *ev = data;
	dbg0("%s() event received: type=%d serial=%s mode_cur=%d mode_r1=%d mode_r2=%d", __func__, type, ev ? ev->serial : "NULL", ev ? ev->mode_cur : -1, ev ? ev->mode_r1 : -1, ev ? ev->mode_r2 : -1);
	if(type != EV_KEYER_MODE || !ev)
		return;
	json_t *event = json_object();
	json_object_set_new(event, "type", json_string("mode_change"));
	json_object_set_new(event, "serial", json_string(ev->serial));
	json_object_set_new(event, "mode_cur", json_integer((json_int_t)ev->mode_cur));
	json_object_set_new(event, "mode_r1", json_integer((json_int_t)ev->mode_r1));
	json_object_set_new(event, "mode_r2", json_integer((json_int_t)ev->mode_r2));
	broadcast_event(api, event);
	json_decref(event);
}

static void on_ws_sub_closed(struct http_connection *hcon, void *data) {
	(void)hcon;
	struct ws_subscriber *sub = data;
	PG_Remove(&sub->node);
	free(sub);
}

static void ws_send_ack_ok(struct http_connection *hcon, json_t *echo) {
	json_t *ack = json_object();
	if(!ack) return;
	json_object_set_new(ack, "type", json_string("ack"));
	json_object_set_new(ack, "status", json_string("ok"));
	if(echo)
		json_object_set(ack, "echo", echo);
	char *dump = json_dumps(ack, JSON_COMPACT);
	if(dump) {
		hs_ws_send_text(hcon, dump, strlen(dump));
		free(dump);
	}
	json_decref(ack);
}

static void ws_send_ack_error(struct http_connection *hcon, const char *error) {
	json_t *ack = json_object();
	if(!ack) return;
	json_object_set_new(ack, "type", json_string("ack"));
	json_object_set_new(ack, "status", json_string("error"));
	json_object_set_new(ack, "error", json_string(error));
	char *dump = json_dumps(ack, JSON_COMPACT);
	if(dump) {
		hs_ws_send_text(hcon, dump, strlen(dump));
		free(dump);
	}
	json_decref(ack);
}

/* Command handlers: return NULL on success, error string on failure */

static const char *cmd_get_device(struct restapi *api, json_t *in, struct device **dev_out) {
	json_t *serial_val = json_object_get(in, "serial");
	if(!json_is_string(serial_val))
		return "missing or invalid 'serial'";
	*dev_out = app_ctx_get_device(api->ctx, json_string_value(serial_val));
	if(!*dev_out)
		return "device not found";
	return NULL;
}

static const char *cmd_play_message(struct restapi *api, json_t *in) {
	struct device *dev;
	const char *e = cmd_get_device(api, in, &dev);
	if(e) return e;
	json_t *message_val = json_object_get(in, "message");
	if(!json_is_integer(message_val))
		return "missing or invalid 'message'";
	json_int_t msg_num = json_integer_value(message_val);
	if(msg_num < 1 || msg_num > 8)
		return "'message' out of range (1-8)";
	mhc_play_message(dev->ctl, (uint8_t)msg_num, NULL, NULL);
	return NULL;
}

static const char *cmd_abort_message(struct restapi *api, json_t *in) {
	struct device *dev;
	const char *e = cmd_get_device(api, in, &dev);
	if(e) return e;
	mhc_abort_message(dev->ctl, NULL, NULL);
	return NULL;
}

static int on_ws_message(struct http_connection *hcon, int opcode, const char *data, size_t len, void *user_data) {
	struct ws_subscriber *sub = user_data;
	if(!sub || !sub->api)
		return -1;

	if(opcode != WS_OP_TEXT)
		return 0;

	json_t *in = load_json_object(data, len);
	if(!in) {
		hs_ws_close(hcon, 1007, "invalid JSON");
		return 0;
	}

	json_t *type_val = json_object_get(in, "type");
	if(json_is_string(type_val) && strcmp(json_string_value(type_val), "command") == 0) {
		json_t *cmd_val = json_object_get(in, "command");
		const char *cmd_str = json_is_string(cmd_val) ? json_string_value(cmd_val) : NULL;
		const char *cmd_err;

		if(!cmd_str) {
			cmd_err = "missing or invalid 'command'";
		} else if(strcmp(cmd_str, "play_message") == 0) {
			cmd_err = cmd_play_message(sub->api, in);
		} else if(strcmp(cmd_str, "abort_message") == 0) {
			cmd_err = cmd_abort_message(sub->api, in);
		} else {
			cmd_err = "unknown command";
		}

		if(cmd_err) {
			err("%s: %s", cmd_str ? cmd_str : "command", cmd_err);
			ws_send_ack_error(hcon, cmd_err);
			json_decref(in);
			return 0;
		}
	}

	ws_send_ack_ok(hcon, in);
	json_decref(in);
	return 0;
}

static int cb_events_ws(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query; (void)body; (void)body_len;
	struct restapi *api = data;

	struct ws_subscriber *sub = w_calloc(1, sizeof(*sub));
	sub->hcon = hcon;
	sub->api = api;

	if(hs_ws_upgrade(hcon, on_ws_message, sub) != 0) {
		free(sub);
		hs_send_error_page(hcon, 400);
		return 0;
	}

	PG_AddTail(&api->ws_subscribers, &sub->node);
	hs_set_close_cb(hcon, on_ws_sub_closed, sub);

	json_t *hello = json_object();
	if(hello) {
		json_object_set_new(hello, "type", json_string("hello"));
		json_object_set_new(hello, "channel", json_string("/api/v1/ws"));
		char *dump = json_dumps(hello, JSON_COMPACT);
		if(dump) {
			hs_ws_send_text(hcon, dump, strlen(dump));
			free(dump);
		}
		json_decref(hello);
	}

	return 0;
}

static int cb_runtime(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query; (void)body; (void)body_len;
	struct restapi *api = data;
	char hostname[256];
	time_t now = time(NULL);
	long uptime = (now >= api->start_time) ? (long)(now - api->start_time) : 0;
	if(gethostname(hostname, sizeof(hostname)) != 0) {
		snprintf(hostname, sizeof(hostname), "unknown");
	} else {
		hostname[sizeof(hostname) - 1] = 0x00;
	}

	json_t *daemon = json_object();
	json_object_set_new(daemon, "name", json_string("mhuxd"));
	json_object_set_new(daemon, "version", json_string(_package_version));
	json_object_set_new(daemon, "logfile", json_string(log_get_file_name()));
	json_object_set_new(daemon, "pid", json_integer((json_int_t)getpid()));
	json_object_set_new(daemon, "uptimeSec", json_integer((json_int_t)uptime));

	json_t *root = json_object();
	json_object_set_new(root, "daemon", daemon);
	json_object_set_new(root, "hostname", json_string(hostname));
	return send_json(hcon, 200, root);
}

static int cb_devices(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	struct restapi *api = data;
	json_t *devices = json_array();

	const struct PGList *list = app_ctx_get_device_list(api->ctx);
	if(list) {
		const struct device *dev;
		PG_SCANLIST(list, dev) {
			const struct mh_info *mhi = mhc_get_mhinfo(dev->ctl);
			json_t *device = json_object();
			json_object_set_new(device, "serial", json_string(dev->serial ? dev->serial : ""));
			json_object_set_new(device, "name", json_string(mhi->type_str));
			json_object_set_new(device, "status", json_string(mhc_state_str(mhc_get_state(dev->ctl))));
			json_object_set_new(device, "verFwMajor", json_integer((json_int_t)mhi->ver_fw_major));
			json_object_set_new(device, "verFwMinor", json_integer((json_int_t)mhi->ver_fw_minor));
			json_object_set_new(device, "verFwBeta", json_boolean(mhi->ver_fw_beta));
			json_object_set_new(device, "verWinkey", json_integer((json_int_t)mhi->ver_winkey));
			json_array_append_new(devices, device);
		}
	}

	json_t *root = json_object();
	json_object_set_new(root, "devices", devices);
	return send_json(hcon, 200, root);
}

static int cb_config_daemon(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	int16_t method = hs_get_method(hcon);

	dbg1("%s %s", __func__, hs_method_str(method));

	if(method != HS_HTTP_GET && method != HS_HTTP_POST && method != HS_HTTP_PUT && method != HS_HTTP_PATCH)
		return send_empty(hcon, 400);

	if(method != HS_HTTP_GET && body && body_len) {
		json_t *root = load_json_object(body, body_len);
		json_t *loglevel = json_object_get(root, "loglevel");
		int rc = json_is_string(loglevel) ? log_set_level_by_str(json_string_value(loglevel)) : -1;
		json_decref(root);
		if(rc == -1)
			return send_empty(hcon, 400);
	}

	json_t *rsp = json_object();
	json_object_set_new(rsp, "loglevel", json_string(log_get_level_str()));
	return send_json(hcon, 200, rsp);
}

static json_t *find_device_in_devices(json_t *devices, const char *serial) {
	size_t i;
	json_t *device;
	json_array_foreach(devices, i, device) {
		json_t *s = json_object_get(device, "serial");
		if(json_is_string(s) && !strcmp(json_string_value(s), serial))
			return device;
	}
	return NULL;
}

static json_t *find_connector_in_connectors(json_t *connectors, int id) {
	size_t i;
	json_t *connector;
	json_array_foreach(connectors, i, connector) {
		json_t *s = json_object_get(connector, "id");
		if(json_is_integer(s) && json_integer_value(s) == id)
			return connector;
	}
	return NULL;
}

/* Respond with the devices section of the config, as {"devices": [...]}. */
static int send_config_devices(struct restapi *api, struct http_connection *hcon) {
	json_t *root = cfgmgrj_build_json(api->cfgmgrj);
	if(!root)
		return send_empty(hcon, 500);
	json_t *devices = json_object_get(root, "devices");
	json_t *rsp = json_object();
	json_object_set_new(rsp, "devices", devices ? json_incref(devices) : json_array());
	json_decref(root);
	return send_json(hcon, 200, rsp);
}

/* Respond with the config of one device, 404 if there is none. */
static int send_config_device(struct restapi *api, struct http_connection *hcon, const char *serial) {
	json_t *root = cfgmgrj_build_json(api->cfgmgrj);
	if(!root)
		return send_empty(hcon, 500);
	json_t *device = find_device_in_devices(json_object_get(root, "devices"), serial);
	if(device)
		send_json(hcon, 200, json_incref(device));
	else
		send_empty(hcon, 404);
	json_decref(root);
	return 0;
}

static int cb_config_connectors(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query;
	struct restapi *api = data;
	int16_t method = hs_get_method(hcon);

	dbg1("%s %s", __func__, hs_method_str(method));

	if(method == HS_HTTP_GET) {
		json_t *root = cfgmgrj_build_json(api->cfgmgrj);
		if(!root)
			return send_empty(hcon, 500);
		json_t *connectors = json_object_get(root, "connectors");
		send_json(hcon, 200, connectors ? json_incref(connectors) : json_array());
		json_decref(root);
		return 0;
	}

	if(method != HS_HTTP_POST)
		return send_empty(hcon, 405);

	json_t *root = load_json_object(body, body_len);
	if(!root)
		return send_empty(hcon, 400);

	int failed = cfgmgrj_add_conn(api->cfgmgrj, root) != 0 || cfgmgrj_save_cfg(api->cfgmgrj) != 0;
	json_decref(root);
	return send_empty(hcon, failed ? 500 : 201);
}

/* Connector ids are positive integers; returns -1 for anything else. */
static int parse_connector_id(const char *s) {
	char *end;
	errno = 0;
	long id = strtol(s, &end, 10);
	if(errno || end == s || *end || id <= 0 || id > INT_MAX)
		return -1;
	return (int)id;
}

static int cb_config_connector(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)query; (void)body; (void)body_len;
	struct restapi *api = data;
	int16_t method = hs_get_method(hcon);
	int id = path ? parse_connector_id(path) : -1;

	dbg1("%s %s id: %s", __func__, hs_method_str(method), path ? path : "NULL");

	if(id < 0)
		return send_empty(hcon, 404);

	if(method == HS_HTTP_GET) {
		json_t *root = cfgmgrj_build_json(api->cfgmgrj);
		if(!root)
			return send_empty(hcon, 500);
		json_t *connector = find_connector_in_connectors(json_object_get(root, "connectors"), id);
		if(connector)
			send_json(hcon, 200, json_incref(connector));
		else
			send_empty(hcon, 404);
		json_decref(root);
		return 0;
	}

	if(method != HS_HTTP_DELETE)
		return send_empty(hcon, 405);

	int rc = cfgmgrj_remove_conn(api->cfgmgrj, id);
	if(rc == -ENOENT)
		return send_empty(hcon, 404);
	if(rc != 0 || cfgmgrj_save_cfg(api->cfgmgrj) != 0)
		return send_empty(hcon, 500);

	return send_empty(hcon, 200);
}

static int cb_config_devices(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query;
	struct restapi *api = data;
	int16_t method = hs_get_method(hcon);

	dbg1("%s %s", __func__, hs_method_str(method));

	if(method == HS_HTTP_GET)
		return send_config_devices(api, hcon);

	if(method != HS_HTTP_POST && method != HS_HTTP_PUT && method != HS_HTTP_PATCH)
		return send_empty(hcon, 400);

	json_t *root = load_json_object(body, body_len);
	if(!root)
		return send_empty(hcon, 400);

	int failed = cfgmgrj_apply_json(api->cfgmgrj, root) != 0 || cfgmgrj_save_cfg(api->cfgmgrj) != 0;
	json_decref(root);
	if(failed)
		return send_empty(hcon, 500);

	return send_config_devices(api, hcon);
}

static int delete_config_device(struct restapi *api, struct http_connection *hcon, const char *serial) {
	switch(cfgmgrj_remove_device(api->cfgmgrj, serial)) {
	case 0:
		break;
	case -ENOENT:
		return send_json_error(hcon, 404, "Keyer not found.");
	case -EBUSY:
		return send_json_error(hcon, 409, "Keyer is connected. Unplug it before removing it.");
	case -EAGAIN:
		return send_json_error(hcon, 409, "Configuration update in progress, try again.");
	default:
		return send_json_error(hcon, 500, "Could not remove keyer.");
	}

	json_t *event = json_object();
	if(event) {
		json_object_set_new(event, "type", json_string("device_removed"));
		json_object_set_new(event, "serial", json_string(serial));
		broadcast_event(api, event);
		json_decref(event);
	}

	if(cfgmgrj_save_cfg(api->cfgmgrj) != 0)
		return send_json_error(hcon, 500, "Keyer removed, but the configuration could not be saved.");

	return send_empty(hcon, 200);
}

static int cb_config_device(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)query;
	struct restapi *api = data;
	int16_t method = hs_get_method(hcon);
	const char *serial = (path && *path) ? path : NULL;

	dbg1("%s %s serial: %s", __func__, hs_method_str(method), serial ? serial : "NULL");

	if(!serial)
		return send_empty(hcon, 404);

	if(method == HS_HTTP_GET)
		return send_config_device(api, hcon, serial);

	if(method == HS_HTTP_DELETE)
		return delete_config_device(api, hcon, serial);

	if(method != HS_HTTP_POST && method != HS_HTTP_PUT && method != HS_HTTP_PATCH)
		return send_empty(hcon, 400);

	json_t *device = load_json_object(body, body_len);
	if(!device)
		return send_empty(hcon, 400);

	json_t *serial_val = json_object_get(device, "serial");
	if(!json_is_string(serial_val)) {
		json_object_set_new(device, "serial", json_string(serial));
	} else if(strcmp(json_string_value(serial_val), serial) != 0) {
		json_decref(device);
		return send_empty(hcon, 400);
	}

	json_t *devices = json_array();
	json_array_append_new(devices, device);
	json_t *root = json_object();
	json_object_set_new(root, "devices", devices);

	int failed = !root || cfgmgrj_apply_json(api->cfgmgrj, root) != 0 || cfgmgrj_save_cfg(api->cfgmgrj) != 0;
	json_decref(root);
	if(failed)
		return send_empty(hcon, 500);

	return send_config_device(api, hcon, serial);
}

static int cb_device_actions(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {

	struct restapi *api = data;
	int16_t method = hs_get_method(hcon);

	/* path is "SERIAL/actions" - extract serial and verify suffix */
	char serial_buf[64];
	const char *serial = NULL;
	if(path && *path) {
		const char *slash = strchr(path, '/');
		if(slash && strcmp(slash + 1, "actions") == 0) {
			size_t len = (size_t)(slash - path);
			if(len > 0 && len < sizeof(serial_buf)) {
				memcpy(serial_buf, path, len);
				serial_buf[len] = '\0';
				serial = serial_buf;
			}
		}
	}

	dbg1("%s %s serial: %s", __func__, hs_method_str(method), serial ? serial : "NULL");

	if(!serial)
		return send_empty(hcon, 404);

	if(method != HS_HTTP_POST)
		return send_json_error(hcon, 405, "Method not allowed");

	if(!body || !body_len)
		return send_json_error(hcon, 400, "Missing request body");

	json_t *root = load_json_object(body, body_len);
	if(!root)
		return send_json_error(hcon, 400, "Invalid JSON");

	json_t *action = json_object_get(root, "action");
	if(!json_is_string(action)) {
		json_decref(root);
		return send_json_error(hcon, 400, "Missing action field");
	}

	const char *action_str = json_string_value(action);
	int rc = -1;
	const char *ok_msg = NULL;
	const char *err_msg = NULL;

	if(strcmp(action_str, "sm_load") == 0) {
		rc = cfgmgrj_sm_load(api->cfgmgrj, serial);
		ok_msg = "Antenna switching settings loaded from device.";
		err_msg = "Could not load antenna switching settings from device.";
	} else if(strcmp(action_str, "sm_store") == 0) {
		rc = cfgmgrj_sm_store(api->cfgmgrj, serial);
		ok_msg = "Antenna switching settings stored to device.";
		err_msg = "Could not store antenna switching settings to device.";
	} else {
		json_decref(root);
		return send_json_error(hcon, 400, "Unknown action");
	}

	json_decref(root);

	json_t *rsp = json_object();
	json_object_set_new(rsp, "status", json_string(rc == 0 ? "ok" : "error"));
	json_object_set_new(rsp, "message", json_string(rc == 0 ? ok_msg : err_msg));
	return send_json(hcon, rc == 0 ? 200 : 500, rsp);
}

/* Registration order matters: the http server dispatches to the first matching path. */
static const struct {
	const char *path;
	http_handler_func func;
} routes[] = {
	{ "/api/v1/runtime", cb_runtime },
	{ "/api/v1/metadata", cb_metadata },
	{ "/api/v1/devices", cb_devices },
	{ "/api/v1/config/daemon", cb_config_daemon },
	{ "/api/v1/config/devices", cb_config_devices },
	{ "/api/v1/config/devices/", cb_config_device },
	{ "/api/v1/config/connectors", cb_config_connectors },
	{ "/api/v1/config/connectors/", cb_config_connector },
	{ "/api/v1/devices/", cb_device_actions },
	{ "/api/v1/ws", cb_events_ws },
};
_Static_assert(ARRAY_SIZE(routes) == NUM_ROUTES, "NUM_ROUTES must match routes[]");

// FIXME: pass ctx alone.
struct restapi *restapi_create(struct app_ctx *ctx, struct http_server *hs, struct cfgmgrj *cfgmgrj) {
	struct restapi *api;
	json_error_t jerr;
	const char *rigtypes_path = JSONDIR "/mh_rigtypes.json";
	const char *displayoptions_path = JSONDIR "/mh_displayoptions.json";

	if(!hs || !cfgmgrj) {
		err("%s() missing http server or config manager", __func__);
		return NULL;
	}

	api = w_calloc(1, sizeof(*api));
	api->ctx = ctx;
	api->hs = hs;
	api->cfgmgrj = cfgmgrj;
	api->start_time = time(NULL);
	PG_NewList(&api->ws_subscribers);

	api->rigtypes = json_load_file(rigtypes_path, 0, &jerr);
	if(!json_is_array(api->rigtypes)) {
		err("%s() failed to load %s: %s", __func__, rigtypes_path,
			jerr.text[0] ? jerr.text : "invalid or empty file");
		goto fail;
	}

	api->displayoptions = json_load_file(displayoptions_path, 0, &jerr);
	if(!json_is_object(api->displayoptions)) {
		err("%s() failed to load %s: %s", __func__, displayoptions_path,
			jerr.text[0] ? jerr.text : "invalid or empty file");
		goto fail;
	}

	api->devicetypes = build_devicetypes_array(api->displayoptions);
	if(!api->devicetypes) {
		err("%s() failed to build devicetypes array", __func__);
		goto fail;
	}

	for(size_t i = 0; i < ARRAY_SIZE(routes); i++) {
		api->handlers[i] = hs_register_handler(hs, routes[i].path, routes[i].func, api);
		if(!api->handlers[i]) {
			err("%s() failed to register handler for %s", __func__, routes[i].path);
			goto fail;
		}
	}

	api->keyer_state_sub = eventbus_subscribe(app_ctx_get_eventbus(api->ctx), EV_KEYER_STATE, ev_keyer_state_cb, api);
	api->keyer_mode_sub  = eventbus_subscribe(app_ctx_get_eventbus(api->ctx), EV_KEYER_MODE,  ev_keyer_mode_cb, api);

	return api;

fail:
	restapi_destroy(api);
	return NULL;
}

void restapi_shutdown(struct restapi *api) {
	if(!api)
		return;

	if(api->keyer_state_sub) {
		eventbus_unsubscribe(api->keyer_state_sub);
		api->keyer_state_sub = NULL;
	}
	if(api->keyer_mode_sub) {
		eventbus_unsubscribe(api->keyer_mode_sub);
		api->keyer_mode_sub = NULL;
	}

	struct ws_subscriber *wsub;
	while((wsub = (void*)PG_FIRSTENTRY(&api->ws_subscribers))) {
		hs_set_close_cb(wsub->hcon, NULL, NULL);
		hs_ws_close(wsub->hcon, 1001, "server shutdown");
		PG_Remove(&wsub->node);
		free(wsub);
	}

	for(size_t i = 0; i < ARRAY_SIZE(api->handlers); i++) {
		if(api->handlers[i]) {
			hs_unregister_handler(api->hs, api->handlers[i]);
			api->handlers[i] = NULL;
		}
	}
}

void restapi_destroy(struct restapi *api) {
	if(!api)
		return;
	restapi_shutdown(api);
	json_decref(api->rigtypes);
	json_decref(api->devicetypes);
	json_decref(api->displayoptions);
	free(api);
}
