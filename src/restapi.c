/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "restapi.h"
#include "http_server.h"
#include "mhinfo.h"
#include "mhcontrol.h"
#include "devmgr.h"
#include "cfgmgrj.h"
#include "util.h"
#include "version.h"
#include "logger.h"

#define MOD_ID "restapi"

struct restapi {
	struct http_server *hs;
	struct cfgmgrj *cfgmgrj;
	struct http_handler *runtime_handler;
	struct http_handler *metadata_handler;
	struct http_handler *devices_handler;
	struct http_handler *config_daemon_handler;
	struct http_handler *config_devices_handler;
	struct http_handler *config_device_handler;
	json_t *rigtypes;
	json_t *devicetypes;
	json_t *displayoptions;
	time_t start_time;
};

extern const char *log_file_name;

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

static const char *device_name_from_type(uint16_t type) {
	for(int i = 0; i < mh_info_map_size; i++) {
		if(mh_info_map[i].type == type)
			return mh_info_map[i].name ? mh_info_map[i].name : "";
	}
	return "";
}

static void attach_display_options(json_t *device, json_t *displayoptions, uint16_t type) {
	if(!device || !displayoptions)
		return;

	json_t *map = json_object_get(displayoptions, "deviceTypeMap");
	if(!map || !json_is_object(map))
		return;

	char type_key[16];
	snprintf(type_key, sizeof(type_key), "%u", (unsigned int)type);
	json_t *set_name = json_object_get(map, type_key);
	if(!set_name || !json_is_string(set_name))
		return;

	json_t *sets = json_object_get(displayoptions, "displayOptionSets");
	if(!sets || !json_is_object(sets))
		return;

	const char *set_name_str = json_string_value(set_name);
	json_t *set = json_object_get(sets, set_name_str);
	if(!set || !json_is_object(set))
		return;

	json_t *bg = json_object_get(set, "displaybackground");
	if(bg && json_is_array(bg))
		json_object_set(device, "displaybackground", bg);

	json_t *ev = json_object_get(set, "displayevent");
	if(ev && json_is_array(ev))
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
			if(device)
				json_decref(device);
			if(flags)
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
	(void)path; (void)query; (void)body; (void)body_len;
	struct restapi *api = data;

	json_t *root = json_object();
	if(!root || !api->devicetypes) {
		if(root)
			json_decref(root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	json_object_set(root, "rigtypes", api->rigtypes);
	json_object_set(root, "devicetypes", api->devicetypes);

	char *payload = json_dumps(root, JSON_COMPACT);
	if(!payload) {
		json_decref(root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	hs_add_rsp_header(hcon, "Cache-Control", "no-store");
	hs_send_response(hcon, 200, "application/json", payload, strlen(payload), NULL, 0);
	free(payload);
	json_decref(root);
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

	json_t *root = json_object();
	json_t *daemon = json_object();
	if(!root || !daemon) {
		if(root)
			json_decref(root);
		if(daemon)
			json_decref(daemon);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	json_object_set_new(daemon, "name", json_string("mhuxd"));
	json_object_set_new(daemon, "version", json_string(_package_version));
	json_object_set_new(daemon, "logfile", json_string(log_file_name ? log_file_name : ""));
	json_object_set_new(daemon, "pid", json_integer((json_int_t)getpid()));
	json_object_set_new(daemon, "uptimeSec", json_integer((json_int_t)uptime));
	json_object_set_new(root, "daemon", daemon);
	json_object_set_new(root, "hostname", json_string(hostname));

	char *payload = json_dumps(root, JSON_COMPACT);
	if(!payload) {
		json_decref(root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	hs_add_rsp_header(hcon, "Cache-Control", "no-store");
	hs_send_response(hcon, 200, "application/json", payload, strlen(payload), NULL, 0);
	free(payload);
	json_decref(root);
	return 0;
}

static int cb_devices(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query; (void)body; (void)body_len; (void)data;

	json_t *root = json_object();
	json_t *devices = json_array();
	if(!root || !devices) {
		if(root)
			json_decref(root);
		if(devices)
			json_decref(devices);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	struct PGList *list = dmgr_get_device_list();
	if(list) {
		struct device *dev;
		PG_SCANLIST(list, dev) {
			json_t *device = json_object();
			const struct mh_info *mhi = mhc_get_mhinfo(dev->ctl);
			if(!device) {
				if(device)
					json_decref(device);
				json_decref(devices);
				json_decref(root);
				hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
				return 0;
			}

			json_object_set_new(device, "serial", json_string(dev->serial ? dev->serial : ""));
			json_object_set_new(device, "name", json_string(device_name_from_type(mhi ? mhi->type : MHT_UNKNOWN)));
			json_object_set_new(device, "status", json_string(mhc_state_str(mhc_get_state(dev->ctl))));
			json_object_set_new(device, "verFwMajor", json_integer((json_int_t)(mhi ? mhi->ver_fw_major : 0)));
			json_object_set_new(device, "verFwMinor", json_integer((json_int_t)(mhi ? mhi->ver_fw_minor : 0)));
			json_object_set_new(device, "verFwBeta", json_boolean((mhi && mhi->ver_fw_beta) ? 1 : 0));
			json_object_set_new(device, "verWinkey", json_integer((json_int_t)(mhi ? mhi->ver_winkey : 0)));

			if(json_array_append_new(devices, device) != 0) {
				json_decref(device);
				json_decref(devices);
				json_decref(root);
				hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
				return 0;
			}
		}
	}

	json_object_set_new(root, "devices", devices);

	char *payload = json_dumps(root, JSON_COMPACT);
	if(!payload) {
		json_decref(root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	hs_add_rsp_header(hcon, "Cache-Control", "no-store");
	hs_send_response(hcon, 200, "application/json", payload, strlen(payload), NULL, 0);
	free(payload);
	json_decref(root);
	return 0;
}

static int cb_config_daemon(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query; (void)data;
	int16_t method = hs_get_method(hcon);

	if(method != HS_HTTP_GET && method != HS_HTTP_POST && method != HS_HTTP_PUT && method != HS_HTTP_PATCH) {
		hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	if((method == HS_HTTP_POST || method == HS_HTTP_PUT || method == HS_HTTP_PATCH) && body && body_len) {
		json_error_t jerr;
		json_t *root = json_loadb(body, body_len, 0, &jerr);
		if(!root || !json_is_object(root)) {
			if(root)
				json_decref(root);
			hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
			return 0;
		}

		json_t *loglevel = json_object_get(root, "loglevel");
		if(!loglevel || !json_is_string(loglevel)) {
			json_decref(root);
			hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
			return 0;
		}

		const char *level_str = json_string_value(loglevel);
		if(level_str && log_set_level_by_str(level_str) == -1) {
			json_decref(root);
			hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
			return 0;
		}

		json_decref(root);
	}

	json_t *rsp = json_object();
	if(!rsp) {
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_object_set_new(rsp, "loglevel", json_string(log_get_level_str()));

	char *payload = json_dumps(rsp, JSON_COMPACT);
	if(!payload) {
		json_decref(rsp);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	hs_add_rsp_header(hcon, "Cache-Control", "no-store");
	hs_send_response(hcon, 200, "application/json", payload, strlen(payload), NULL, 0);
	free(payload);
	json_decref(rsp);
	return 0;
}

static json_t *find_device_in_devices(json_t *devices, const char *serial) {
	if(!devices || !json_is_array(devices) || !serial)
		return NULL;
	for(size_t i = 0; i < json_array_size(devices); i++) {
		json_t *device = json_array_get(devices, i);
		json_t *s = json_object_get(device, "serial");
		if(s && json_is_string(s) && !strcmp(json_string_value(s), serial))
			return device;
	}
	return NULL;
}

static int send_json_payload(struct http_connection *hcon, json_t *root) {
	char *payload = json_dumps(root, JSON_COMPACT);
	if(!payload) {
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	hs_add_rsp_header(hcon, "Cache-Control", "no-store");
	hs_send_response(hcon, 200, "application/json", payload, strlen(payload), NULL, 0);
	free(payload);
	return 0;
}

static int cb_config_devices(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query;
	struct restapi *api = data;
	int16_t method = hs_get_method(hcon);

	if(!api || !api->cfgmgrj) {
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	if(method == HS_HTTP_GET) {
		json_t *root = cfgmgrj_build_json(api->cfgmgrj);
		if(!root) {
			hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
			return 0;
		}
		json_t *devices = json_object_get(root, "devices");
		json_t *connectors = json_object_get(root, "connectors");
		json_t *rsp = json_object();
		if(!rsp) {
			json_decref(root);
			hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
			return 0;
		}
		json_object_set_new(rsp, "devices", devices ? json_incref(devices) : json_array());
		if(connectors && json_is_array(connectors))
			json_object_set_new(rsp, "connectors", json_incref(connectors));
		json_decref(root);
		send_json_payload(hcon, rsp);
		json_decref(rsp);
		return 0;
	}

	if(method != HS_HTTP_POST && method != HS_HTTP_PUT && method != HS_HTTP_PATCH) {
		hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	if(!body || !body_len) {
		hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	json_error_t jerr;
	json_t *root = json_loadb(body, body_len, 0, &jerr);
	if(!root || !json_is_object(root)) {
		if(root)
			json_decref(root);
		hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	if(cfgmgrj_apply_json(api->cfgmgrj, root) != 0 || cfgmgrj_save_cfg(api->cfgmgrj) != 0) {
		json_decref(root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_decref(root);

	json_t *rsp_root = cfgmgrj_build_json(api->cfgmgrj);
	if(!rsp_root) {
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_t *devices = json_object_get(rsp_root, "devices");
	json_t *connectors = json_object_get(rsp_root, "connectors");
	json_t *rsp = json_object();
	if(!rsp) {
		json_decref(rsp_root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_object_set_new(rsp, "devices", devices ? json_incref(devices) : json_array());
	if(connectors && json_is_array(connectors))
		json_object_set_new(rsp, "connectors", json_incref(connectors));
	json_decref(rsp_root);
	send_json_payload(hcon, rsp);
	json_decref(rsp);
	return 0;
}

static int cb_config_device(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)query;
	struct restapi *api = data;
	int16_t method = hs_get_method(hcon);
	const char *serial = (path && *path) ? path : NULL;

	if(!api || !api->cfgmgrj || !serial) {
		hs_send_response(hcon, 404, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	if(method == HS_HTTP_GET) {
		json_t *root = cfgmgrj_build_json(api->cfgmgrj);
		if(!root) {
			hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
			return 0;
		}
		json_t *devices = json_object_get(root, "devices");
		json_t *device = find_device_in_devices(devices, serial);
		if(!device) {
			json_decref(root);
			hs_send_response(hcon, 404, "application/json", "{}", 2, NULL, 0);
			return 0;
		}
		json_t *rsp = json_deep_copy(device);
		json_decref(root);
		if(!rsp) {
			hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
			return 0;
		}
		send_json_payload(hcon, rsp);
		json_decref(rsp);
		return 0;
	}

	if(method != HS_HTTP_POST && method != HS_HTTP_PUT && method != HS_HTTP_PATCH) {
		hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	if(!body || !body_len) {
		hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	json_error_t jerr;
	json_t *device = json_loadb(body, body_len, 0, &jerr);
	if(!device || !json_is_object(device)) {
		if(device)
			json_decref(device);
		hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	json_t *serial_val = json_object_get(device, "serial");
	if(serial_val && json_is_string(serial_val)) {
		if(strcmp(json_string_value(serial_val), serial) != 0) {
			json_decref(device);
			hs_send_response(hcon, 400, "application/json", "{}", 2, NULL, 0);
			return 0;
		}
	} else {
		json_object_set_new(device, "serial", json_string(serial));
	}

	json_t *root = json_object();
	json_t *devices = json_array();
	if(!root || !devices) {
		if(root)
			json_decref(root);
		if(devices)
			json_decref(devices);
		json_decref(device);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_array_append_new(devices, device);
	json_object_set_new(root, "devices", devices);

	if(cfgmgrj_apply_json(api->cfgmgrj, root) != 0 || cfgmgrj_save_cfg(api->cfgmgrj) != 0) {
		json_decref(root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_decref(root);

	json_t *rsp_root = cfgmgrj_build_json(api->cfgmgrj);
	if(!rsp_root) {
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_t *rsp_devices = json_object_get(rsp_root, "devices");
	json_t *rsp_device = find_device_in_devices(rsp_devices, serial);
	if(!rsp_device) {
		json_decref(rsp_root);
		hs_send_response(hcon, 404, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	json_t *rsp = json_deep_copy(rsp_device);
	json_decref(rsp_root);
	if(!rsp) {
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}
	send_json_payload(hcon, rsp);
	json_decref(rsp);
	return 0;
}

struct restapi *restapi_create(struct http_server *hs, struct cfgmgrj *cfgmgrj) {
    struct restapi *api = NULL;
    json_error_t jerr;
	const char *rigtypes_path = JSONDIR "/mh_rigtypes.json";
	const char *displayoptions_path = JSONDIR "/mh_displayoptions.json";

    if(!hs) {
        err("%s() missing http server", __func__);
        return NULL;
    }

    api = w_calloc(1, sizeof(*api));
    if(!api) {
        err("%s() out of memory", __func__);
        goto fail;
    }

    api->rigtypes = json_load_file(rigtypes_path, 0, &jerr);
    if(!api->rigtypes || !json_is_array(api->rigtypes)) {
        err("%s() failed to load %s: %s", __func__, rigtypes_path,
            jerr.text[0] ? jerr.text : "invalid or empty file");
        goto fail;
    }

	api->displayoptions = json_load_file(displayoptions_path, 0, &jerr);
	if(!api->displayoptions || !json_is_object(api->displayoptions)) {
		err("%s() failed to load %s: %s", __func__, displayoptions_path,
			jerr.text[0] ? jerr.text : "invalid or empty file");
		goto fail;
	}

	api->devicetypes = build_devicetypes_array(api->displayoptions);
    if(!api->devicetypes) {
        err("%s() failed to build devicetypes array", __func__);
        goto fail;
    }

    api->hs = hs;
	api->cfgmgrj = cfgmgrj;
    api->start_time = time(NULL);
	api->runtime_handler = hs_register_handler(hs, "/api/v1/runtime", cb_runtime, api);
	api->metadata_handler = hs_register_handler(hs, "/api/v1/metadata", cb_metadata, api);
	api->devices_handler = hs_register_handler(hs, "/api/v1/devices", cb_devices, api);
	api->config_daemon_handler = hs_register_handler(hs, "/api/v1/config/daemon", cb_config_daemon, api);
	api->config_devices_handler = hs_register_handler(hs, "/api/v1/config/devices", cb_config_devices, api);
	api->config_device_handler = hs_register_handler(hs, "/api/v1/config/devices/", cb_config_device, api);
	if(!api->runtime_handler || !api->metadata_handler || !api->devices_handler || !api->config_daemon_handler ||
	   !api->config_devices_handler || !api->config_device_handler) {
        err("%s() failed to register rest api handlers", __func__);
        goto fail;
    }

    return api;

fail:
    if(api) {
        if(api->runtime_handler)
            hs_unregister_handler(hs, api->runtime_handler);
        if(api->metadata_handler)
            hs_unregister_handler(hs, api->metadata_handler);
		if(api->devices_handler)
			hs_unregister_handler(hs, api->devices_handler);
		if(api->config_daemon_handler)
			hs_unregister_handler(hs, api->config_daemon_handler);
		if(api->config_devices_handler)
			hs_unregister_handler(hs, api->config_devices_handler);
		if(api->config_device_handler)
			hs_unregister_handler(hs, api->config_device_handler);
        if(api->rigtypes)
            json_decref(api->rigtypes);
        if(api->devicetypes)
            json_decref(api->devicetypes);
		if(api->displayoptions)
			json_decref(api->displayoptions);
        free(api);
    }
    return NULL;
}


void restapi_destroy(struct restapi *api) {
	if(!api)
		return;
	if(api->runtime_handler)
		hs_unregister_handler(api->hs, api->runtime_handler);
	if(api->metadata_handler)
		hs_unregister_handler(api->hs, api->metadata_handler);
	if(api->devices_handler)
		hs_unregister_handler(api->hs, api->devices_handler);
	if(api->config_daemon_handler)
		hs_unregister_handler(api->hs, api->config_daemon_handler);
	if(api->config_devices_handler)
		hs_unregister_handler(api->hs, api->config_devices_handler);
	if(api->config_device_handler)
		hs_unregister_handler(api->hs, api->config_device_handler);
	if(api->rigtypes)
		json_decref(api->rigtypes);
	if(api->devicetypes)
		json_decref(api->devicetypes);
	if(api->displayoptions)
		json_decref(api->displayoptions);
	free(api);
}