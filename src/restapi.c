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
#include "util.h"
#include "version.h"
#include "logger.h"

#define MOD_ID "restapi"

struct restapi {
	struct http_server *hs;
	struct http_handler *runtime_handler;
	struct http_handler *metadata_handler;
	json_t *rigtypes;
	json_t *devices;
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

static json_t *build_devices_array(void) {
	json_t *devices = json_array();
	if(!devices)
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
			json_decref(devices);
			return NULL;
		}

		if(json_object_set_new(device, "type", json_integer((json_int_t)info->type)) != 0 ||
		   json_object_set_new(device, "name", json_string(info->name)) != 0 ||
		   json_object_set_new(device, "flags", flags) != 0) {
			json_decref(device);
			json_decref(devices);
			return NULL;
		}

		for(size_t f = 0; f < ARRAY_SIZE(mh_flag_names); f++) {
			if(info->flags & mh_flag_names[f].flag) {
				if(json_array_append_new(flags, json_string(mh_flag_names[f].name)) != 0) {
					json_decref(device);
					json_decref(devices);
					return NULL;
				}
			}
		}

		if(json_array_append_new(devices, device) != 0) {
			json_decref(devices);
			return NULL;
		}
	}

	return devices;
}

static int cb_metadata(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query; (void)body; (void)body_len;
	struct restapi *api = data;

	json_t *root = json_object();
	if(!root || !api->devices) {
		if(root)
			json_decref(root);
		hs_send_response(hcon, 500, "application/json", "{}", 2, NULL, 0);
		return 0;
	}

	json_object_set(root, "rigtypes", api->rigtypes);
	json_object_set(root, "devices", api->devices);

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

struct restapi *restapi_create(struct http_server *hs) {
    struct restapi *api = NULL;
    json_error_t jerr;
    const char *rigtypes_path = JSONDIR "/mh_rigtypes.json";

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

    api->devices = build_devices_array();
    if(!api->devices) {
        err("%s() failed to build devices array", __func__);
        goto fail;
    }

    api->hs = hs;
    api->start_time = time(NULL);
    api->runtime_handler = hs_register_handler(hs, "/api/v1/runtime", cb_runtime, api);
    api->metadata_handler = hs_register_handler(hs, "/api/v1/metadata", cb_metadata, api);
    if(!api->runtime_handler || !api->metadata_handler) {
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
        if(api->rigtypes)
            json_decref(api->rigtypes);
        if(api->devices)
            json_decref(api->devices);
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
	if(api->rigtypes)
		json_decref(api->rigtypes);
	if(api->devices)
		json_decref(api->devices);
	free(api);
}