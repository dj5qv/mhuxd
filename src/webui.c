/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "clearsilver/ClearSilver.h"
#include "webui.h"
#include "util.h"
#include "http_server.h"
#include "pglist.h"
#include "util.h"
#include "logger.h"
#include "obuf.h"
#include "cfgmgr.h"
#include "http_parse_query.h"
#include "mhinfo.h"

struct webui {
	HDF *hdf;
	CSPARSE *parse;
	struct http_server *hs;
	struct http_handler *handler_cs;
	struct http_handler *handler_redir;
	struct cfgmgr *cfgmgr;
};

static const char cs_path[] = WEBUIDIR "/cs";
static const char static_path[] = WEBUIDIR "/static";
static const char mhuxd_hdf[] = HDFDIR "/mhuxd.hdf";

static NEOERR* out_func(void *user_data, char *data) {
	struct obuf *obuf = user_data;

	size_t len = strlen(data);

	while(len > obuf_avail(obuf)) {
		size_t needed = len - obuf_avail(obuf);
		obuf = obuf_realloc(obuf, obuf->capacity + (needed / 4096 + 2) * 4096);
	}

	memcpy(obuf->data + obuf->size, data, len);
	obuf->size += len;

        return STATUS_OK;
}


static const char *compose_ptt_str(HDF *hdf, uint16_t type, uint8_t qsk) {
	int ptt1, ptt2;

	ptt1 = hdf_get_int_value(hdf, "ptt1", 0);
	ptt2 = hdf_get_int_value(hdf, "ptt2", 0);

	switch(type) {
	case MHT_CK:
		return ptt2 ? "ptt" : "qsk";
	case MHT_DK:
	case MHT_DK2:
		if(ptt2)
			return "ptt";
		return qsk ? "qsk" : "semi";
	case MHT_MK:
		if(ptt1 && ptt2)
			return "ptt12";
		if(ptt1)
			return "ptt1";
		if(ptt2)
			return "ptt2";
		return "qsk";
	case MHT_MK2:
	case MHT_MK2R:
	case MHT_MK2Rp:
		if(ptt1 && ptt2)
			return "ptt12";
		if(ptt1)
			return "ptt1";
		if(ptt2)
			return "ptt2";
		if(qsk)
			return "qsk";
		return "semi";
	default:
		err("(webui) %s() unknown keyer type %d", __func__, type);
	}

	return "_invalid_";
}

static const char *mk2_compose_mic_str(HDF *hdf, uint16_t type) {
	if(type != MHT_MK2)
		return NULL;

	if(hdf_get_int_value(hdf, "micSelAuto", 0) == 1)
		return "auto";
	if(hdf_get_int_value(hdf, "micSelFront", 0) == 1)
		return "front";
	return "rear";
}

static int encode_meta_settings(HDF *hdf) {
	NEOERR *err;
	int rval = 0;
	HDF *klist = hdf_get_obj(hdf, "mhuxd.keyer");
	if(!klist)
		return 0;

	HDF *knod;
	for(knod = hdf_obj_child(klist); knod; knod = hdf_obj_next(knod)) {
		HDF *frbase, *meta, *tmp;
		uint16_t type;
		uint8_t qsk;
		err = hdf_get_node(hdf, "mhuxd.webui.meta.keyer", &tmp);
		nerr_ignore(&err);
		err = hdf_get_node(tmp, hdf_obj_name(knod), &meta);
		nerr_ignore(&err);

		if(!meta) {
			rval++;
			err("(webui) %s() could not create hdf object!", __func__);
			continue;
		}

		qsk = hdf_get_int_value(knod, "param.r1Qsk", 0);
		type = hdf_get_int_value(knod, "type", MHT_UNKNOWN);
		if(type == MHT_UNKNOWN) {
			continue;
		}

		// MK2 MicSel
		if(type == MHT_MK2) {
			frbase = hdf_get_obj(knod, "param");
			if(frbase) {
				err = hdf_set_value(meta, "r1.mk2micsel", mk2_compose_mic_str(frbase, type));
				nerr_ignore(&err);
			}
		}

		// PTT CW
		frbase = hdf_get_obj(knod, "param.r1FrBase_Cw");
		if(frbase) {
			// dbg1("(webui) compose meta for type %d r1 mode cw", type);
			err = hdf_set_value(meta, "r1.ptt_cw", compose_ptt_str(frbase, type, qsk));
			nerr_ignore(&err);
		}

		// These keyers support CW PTT settings only
		if(type == MHT_DK || type == MHT_DK2 || type == MHT_CK)
			continue;

		// PTT VOICE
		frbase = hdf_get_obj(knod, "param.r1FrBase_Voice");
		if(frbase) {
			// dbg1("(webui) compose meta for type %d r1 mode voice", type);
			err = hdf_set_value(meta, "r1.ptt_voice", compose_ptt_str(frbase,type, qsk));
			nerr_ignore(&err);
		}

		// PTT DIGITAL
		frbase = hdf_get_obj(knod, "param.r1FrBase_Digital");
		if(frbase) {
			// dbg1("(webui) compose meta for type %d r1 mode digital", type);
			err = hdf_set_value(meta, "r1.ptt_digital", compose_ptt_str(frbase,type, qsk));
			nerr_ignore(&err);
		}

		if(type != MHT_MK2R && type != MHT_MK2Rp)
			continue;
		// 2nd radio

		qsk = hdf_get_int_value(knod, "param.r2Qsk", 0);

		// PTT CW
		frbase = hdf_get_obj(knod, "param.r2FrBase_Cw");

		if(frbase) {
			// dbg1("(webui) compose meta for type %d r2 mode cw", type);
			err = hdf_set_value(meta, "r2.ptt_cw", compose_ptt_str(frbase,type, qsk));
			nerr_ignore(&err);
		}

		// PTT VOICE
		frbase = hdf_get_obj(knod, "param.r2FrBase_Voice");
		if(frbase) {
			// dbg1("(webui) compose meta for type %d r2 mode voice", type);
			err = hdf_set_value(meta, "r2.ptt_voice", compose_ptt_str(frbase,type, qsk));
			nerr_ignore(&err);
		}

		// PTT DIGITAL
		frbase = hdf_get_obj(knod, "param.r2FrBase_Digital");
		if(frbase) {
			// dbg1("(webui) compose meta for type %d r2 mode digital", type);
			err = hdf_set_value(meta, "r2.ptt_digital", compose_ptt_str(frbase,type, qsk));
			nerr_ignore(&err);
		}
	}

	return rval;
}

struct decompose_ptt_map {
	uint16_t keyer_type;
	const char *ptt_str;
	unsigned ptt1 : 1;
	unsigned ptt2 : 1;
	unsigned qsk : 1;
};

struct decompose_ptt_map decompose_ptt_map[] = {
{ MHT_MK2, "ptt1", 1, 0, 0 },
{ MHT_MK2, "ptt2", 0, 1, 0 },
{ MHT_MK2, "ptt12", 1, 1, 0 },
{ MHT_MK2, "semi", 0, 0, 0 },
{ MHT_MK2, "qsk", 0, 0, 1 },

{ MHT_MK, "ptt1", 1, 0, 0 },
{ MHT_MK, "ptt2", 0, 1, 0 },
{ MHT_MK, "ptt12", 1, 1, 0 },
{ MHT_MK, "qsk", 0, 0, 1 },

{ MHT_DK2, "ptt", 0, 1, 0 },
{ MHT_DK2, "semi", 0, 0, 0 },
{ MHT_DK2, "qsk", 0, 0, 1 },

{ MHT_CK, "ptt", 0, 1, 0 },
{ MHT_CK, "qsk", 0, 0, 1 },

{ MHT_CK, "ptt", 0, 1, 0 },
{ MHT_CK, "noptt", 0, 0, 0 },

{ MHT_MK2R, "ptt1", 1, 0, 0 },
{ MHT_MK2R, "ptt2", 0, 1, 0 },
{ MHT_MK2R, "ptt12", 1, 1, 0 },
{ MHT_MK2R, "semi", 0, 0, 0 },
{ MHT_MK2R, "qsk", 0, 0, 1 },

{ MHT_MK2Rp, "ptt1", 1, 0, 0 },
{ MHT_MK2Rp, "ptt2", 0, 1, 0 },
{ MHT_MK2Rp, "ptt12", 1, 1, 0 },
{ MHT_MK2Rp, "semi", 0, 0, 0 },
{ MHT_MK2Rp, "qsk", 0, 0, 1 },
};


static int decompose_ptt_str(HDF *hdf, uint16_t type, const char *chan_str, const char *key, const char *val) {
	NEOERR *err;
	char base_buf[32], qsk_buf[16];
	uint16_t is_cw = 0;

	strcpy(base_buf, chan_str);

	if(!strcmp(key, "ptt_cw")) {
		strcpy(base_buf+2, "FrBase_Cw");
		is_cw = 1;
	}
	if(!strcmp(key, "ptt_voice"))
		strcpy(base_buf+2, "FrBase_Voice");
	if(!strcmp(key, "ptt_digital"))
		strcpy(base_buf+2, "FrBase_Digital");

	strcpy(qsk_buf, chan_str);
	strcpy(qsk_buf + 2, "Qsk");

	uint16_t i, base_len;
	base_len = strlen(base_buf);

	for(i = 0; i < ARRAY_SIZE(decompose_ptt_map); i++) {
		if(type == decompose_ptt_map[i].keyer_type && !strcmp(val, decompose_ptt_map[i].ptt_str)) {
			strcpy(base_buf + base_len, ".ptt1");
			err = hdf_set_int_value(hdf, base_buf, decompose_ptt_map[i].ptt1);
			nerr_ignore(&err);

			strcpy(base_buf + base_len, ".ptt2");
			err = hdf_set_int_value(hdf, base_buf, decompose_ptt_map[i].ptt2);
			nerr_ignore(&err);

			if(is_cw) {
				err = hdf_set_int_value(hdf, qsk_buf, decompose_ptt_map[i].qsk);
				nerr_ignore(&err);
			}
		}
	}

	return 0;
}

static int mk2_decompose_mic_str(HDF *hdf, uint16_t type, const char *key, const char *val) {
	NEOERR *err;

	err("%s()", __func__);

	if(type != MHT_MK2)
		return -1;

	if(strcmp(key, "mk2micsel"))
		return 0;

	if(!strcmp(val, "auto")) {
		err = hdf_set_int_value(hdf, "micSelAuto", 1);
		nerr_ignore(&err);
		err = hdf_set_int_value(hdf, "micSelFront", 0);
		nerr_ignore(&err);
		return 0;
	}

	if(!strcmp(val, "front")) {
		err = hdf_set_int_value(hdf, "micSelAuto", 0);
		nerr_ignore(&err);
		err = hdf_set_int_value(hdf, "micSelFront", 1);
		nerr_ignore(&err);
		return 0;
	}

	if(!strcmp(val, "rear")) {
		err = hdf_set_int_value(hdf, "micSelAuto", 0);
		nerr_ignore(&err);
		err = hdf_set_int_value(hdf, "micSelFront", 0);
		nerr_ignore(&err);
		return 0;
	}
	return -1;
}

static int decode_meta_settings(HDF *hdf, HDF *metaset) {
	NEOERR *err;
	HDF *kmeta;
	char buf[128];
	for(kmeta = hdf_get_child(metaset, "mhuxd.webui.meta.keyer"); kmeta; kmeta = hdf_obj_next(kmeta)) {
		const char *unit = hdf_obj_name(kmeta);
		if(!unit)
			continue;

		uint16_t type = hdf_get_int_value(kmeta, "type", MHT_UNKNOWN);
		if(type == MHT_UNKNOWN) {
			warn("(webui) %s() unknown keyer type %d!", __func__, type);
			continue;
		}

		HDF *param;
		snprintf(buf, sizeof(buf)-1, "mhuxd.webui.session.set.mhuxd.keyer.%s.param", unit);
		err = hdf_get_node(hdf, buf, &param);
		if(err != STATUS_OK) {
			nerr_ignore(&err);
			continue;
		}

		HDF *chan;
		for(chan = hdf_obj_child(kmeta); chan; chan = hdf_obj_next(chan)) {
			const char *chan_str = hdf_obj_name(chan);
			if(strcmp(chan_str, "r1") && strcmp(chan_str, "r2"))
				continue;
			HDF *opt;
			for(opt = hdf_obj_child(chan); opt; opt = hdf_obj_next(opt)) {
				// err("-->checking %s/%s", hdf_obj_name(opt), hdf_obj_value(opt));
				if(!strncmp(hdf_obj_name(opt), "ptt_", 4))
					decompose_ptt_str(param, type, chan_str, hdf_obj_name(opt), hdf_obj_value(opt));
				if(!strcmp(hdf_obj_name(opt), "mk2micsel"))
					mk2_decompose_mic_str(param, type, hdf_obj_name(opt), hdf_obj_value(opt));

			}
		}
	}

	return 0;
}

static NEOERR *dot2ul(const char *in, char **out) {
	char *p = w_strdup(in);
	*out = p;

	while(*p) {
		if(*p == '.')
			*p = '_';
		p++;
	}
	return STATUS_OK;
}

static int cb_redirect_home(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	(void)path; (void)query; (void)body; (void)body_len;(void)data;

	dbg1("(webui) redirect / to /cs/home.cs");
	hs_add_rsp_header(hcon, "Location", "/cs/home.cs");
	hs_send_response(hcon, 301, "text/html", NULL, 0, NULL, 0);
	return 0;
}

static int cb_cs(struct http_connection *hcon, const char *path, const char *query,
		 const char *body, uint32_t body_len, void *data) {
	struct webui *webui = data;
	NEOERR *err;
	uint16_t http_error = 500;

	dbg1("%s() query: %s", __func__, query);
	if(body && body_len)
		dbg1("%s() post:  %s", __func__, body);

	if(chdir(cs_path)) {
		err_e(errno, "(webui) Could access directory %s!", cs_path);
		hs_send_error_page(hcon, 404);
		return 0;
	}

	dbg1("(webui) cb_cs() path: %s query: %s", path, query);


	if(webui->hdf)
		hdf_destroy(&webui->hdf);

	if(webui->parse) 
		cs_destroy(&webui->parse);

	// initialize

	err = hdf_init(&webui->hdf);
	if(err != STATUS_OK) 
		goto failed;

	err = hdf_read_file(webui->hdf, mhuxd_hdf);
	if(err != STATUS_OK) 
		goto failed;


	// merge query parameter

	HDF *qhdf;
	err = hdf_get_node(webui->hdf, "mhuxd.webui.session", &qhdf);
	if(err != STATUS_OK)
		goto failed;

	if(query && *query) {
		// need a non-const copy
		char *q = w_strdup(query);
		http_parse_query(qhdf, q);
		free(q);
	}

	if(body && body_len) {
		char *q = w_strdup(body);
		http_parse_query(qhdf, q);
		free(q);
	}


	// may need to perform cfg changes

	HDF *metaset_hdf = hdf_get_obj(webui->hdf, "mhuxd.webui.session.metaset");
	if(metaset_hdf && hdf_get_value(webui->hdf, "mhuxd.webui.session.SaveButton", NULL)) {
		if(err == STATUS_OK)
			decode_meta_settings(webui->hdf, metaset_hdf);
		else
			nerr_ignore(&err);
	}

	enum {
		ACTION_NONE = 0,
		ACTION_SAVE = 1,
		ACTION_REMOVE = 2,
		ACTION_MODIFY = 3,
		ACTION_SM_LOAD = 4,
		ACTION_SM_STORE = 5
	};

	int action = ACTION_NONE;
	if(hdf_get_value(webui->hdf, "mhuxd.webui.session.SaveButton", NULL))
		action = ACTION_SAVE;
	if(hdf_get_value(webui->hdf, "mhuxd.webui.session.Remove", NULL))
		action = ACTION_REMOVE;
	if(hdf_get_value(webui->hdf, "mhuxd.webui.session.Modify", NULL))
		action = ACTION_MODIFY;
	if(hdf_get_value(webui->hdf, "mhuxd.webui.session.SmLoad", NULL))
		action = ACTION_SM_LOAD;
	if(hdf_get_value(webui->hdf, "mhuxd.webui.session.SmStore", NULL))
		action = ACTION_SM_STORE;

	
	HDF *set_hdf = hdf_get_obj(webui->hdf, "mhuxd.webui.session.set");
	if(action == ACTION_SAVE && set_hdf) {
		if(cfgmgr_apply_cfg(webui->cfgmgr, (void*)set_hdf, CFGMGR_APPLY_ADD)) {
			warn("(webui) could not apply config change (completely)!");
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.error", 
					    "Could not apply configuration change! Check log file for details.");
			nerr_ignore(&err);
		}
	}

	HDF *mod_hdf = hdf_get_obj(webui->hdf, "mhuxd.webui.session.modify");
	if(action == ACTION_MODIFY && mod_hdf) {
		if(cfgmgr_modify(webui->cfgmgr, (void*)mod_hdf)) {
			warn("(webui) could not apply config change (completely)!");
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.error", 
					    "Could not apply configuration change! Check log file for details.");
			nerr_ignore(&err);
		}
	}

	if(action == ACTION_REMOVE && mod_hdf) {
		if(cfgmgr_remove(webui->cfgmgr, (void*)mod_hdf)) {
			warn("(webui) could not apply config change (completely)!");
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.error", 
					    "Could not apply configuration change! Check log file for details.");
			nerr_ignore(&err);
		}
	}

	if(action == ACTION_SM_LOAD) {
		if(cfgmgr_sm_load(hdf_get_value(webui->hdf, "mhuxd.webui.session.unit", "Unkown"))) {
			warn("(webui) could not load antenna switching settings!");
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.error", 
					    "Could not load antenna switching settings! Check log file for details.");
			nerr_ignore(&err);
		} else {
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.info", 
					    "Antenna switching settings loaded!");
			nerr_ignore(&err);
		}
	}

	if(action == ACTION_SM_STORE) {
		if(cfgmgr_sm_store(hdf_get_value(webui->hdf, "mhuxd.webui.session.unit", "Unkown"))) {
			warn("(webui) could not store antenna switching settings!");
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.error", 
					    "Could not store antenna switching settings! Check log file for details.");
			nerr_ignore(&err);
		} else {
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.info", 
					    "Antenna switching settings stored!");
			nerr_ignore(&err);
		}
	}
	
	if(action != ACTION_NONE) {
		if(cfgmgr_save_cfg(webui->cfgmgr)) {
			err = hdf_set_value(webui->hdf, "mhuxd.webui.notify.error",
				    "Could not save configuration! Check log file for details.");
			nerr_ignore(&err);
		}
	}

	// merge current configuration
	cfgmgr_merge_cfg(webui->cfgmgr, (struct cfg *)webui->hdf);

	// generate meta settings from keyer parameters
	encode_meta_settings(webui->hdf);

	// add keyer page(s)
	HDF *keyer_hdf = hdf_get_child(webui->hdf, "mhuxd.run.keyer");
	HDF *tabs_hdf = hdf_get_obj(webui->hdf, "mhuxd.webui.tabs");

	while(keyer_hdf && tabs_hdf) {
		const char *name, *serial;
		int type;

		type = hdf_get_int_value(keyer_hdf, "info.type", MHT_UNKNOWN);
		name = hdf_get_value(keyer_hdf, "info.name", NULL);
		serial = hdf_obj_name(keyer_hdf);
		if(type != MHT_UNKNOWN && name && serial) {
			HDF *keyer_page_hdf;
			err = hdf_get_node(tabs_hdf, serial, &keyer_page_hdf); nerr_ignore(&err);
			if(keyer_page_hdf) {
				err = hdf_set_value(keyer_page_hdf, "page", "keyer");
				err = hdf_set_value(keyer_page_hdf, "display", name);
				err = hdf_set_value(keyer_page_hdf, "unit", serial);
			}
		}
		keyer_hdf = hdf_obj_next(keyer_hdf);
	}


#if 0
	err = hdf_write_file_atomic(webui->hdf, "/tmp/.mhuxd-full.hdf");
	nerr_ignore(&err);
#endif


#if 0
	STRING str;
	string_init(&str);
	hdf_dump_str(webui->hdf, "", 0, &str);
	//hdf_dump_str(qhdf, "", 0, &str);
	dbg1("%s dump:", __func__);
	dbg1("%s", str.buf);
	string_clear(&str);
#endif


	// Init CS & parse

	err = cs_init(&webui->parse, webui->hdf);
	if(err != STATUS_OK) 
		goto failed;

	err = cs_register_strfunc(webui->parse, "dot2ul", dot2ul);
	if(err != STATUS_OK) 
		goto failed;

        err = cs_parse_file(webui->parse, path);
	if(err != STATUS_OK) {
		if(nerr_match(err, NERR_NOT_FOUND))
			http_error = 404;
		goto failed;
	}


	// Render

	struct obuf *obuf = obuf_alloc(4096);

        err = cs_render(webui->parse, obuf, out_func);

	if(err != STATUS_OK) 
		goto failed;

	hs_send_response(hcon, 200, "text/html", obuf->data, obuf->size, NULL, 0);

	obuf_free(obuf);

	return 0;

 failed:
	{
	STRING str;
	string_init(&str);
	nerr_error_string(err, &str);
	err("(webui) %s() %s", __func__, str.buf);
	string_clear(&str);
	nerr_ignore(&err);
	hs_send_error_page(hcon, http_error);
	return 0;
	}
}

struct webui * webui_create(struct http_server *hs, struct cfgmgr *cfgmgr) {
	struct webui *webui;
	NEOERR *err;

	if(chdir(cs_path)) {
		err_e(errno, "(webui) Could access directory %s!", cs_path);
		return NULL;
	}

	webui = w_calloc(1, sizeof(*webui));

	webui->hs = hs;
	webui->cfgmgr = cfgmgr;

	err = hdf_init(&webui->hdf);
	if(err != STATUS_OK) {
		err("(webui) hdf_init() failed!");
		nerr_ignore(&err);
		goto fail;
	}

	err = cs_init(&webui->parse, webui->hdf);
	if(err != STATUS_OK) {
		err("(webui) cs_init() failed!");
		nerr_ignore(&err);
		goto fail;
	}

	err = hdf_read_file(webui->hdf, mhuxd_hdf);
	if(err != STATUS_OK) {
		err("(webui) Could not read file %s", mhuxd_hdf);
		nerr_ignore(&err);
		goto fail;
	}

	webui->handler_cs = hs_register_handler(hs, "/cs/", cb_cs, webui);
	webui->handler_redir = hs_register_handler(hs, "/", cb_redirect_home, webui);

	hs_add_directory_map(hs, "/static/", static_path);

	dbg0("(webui) WebUI started");

	return webui;

	fail:
	hdf_destroy(&webui->hdf);
	cs_destroy(&webui->parse);
	free(webui);
	return NULL;
}

void webui_destroy(struct webui *webui) {
	if(!webui)
		return;
	hs_unregister_handler(webui->hs, webui->handler_cs);
	hs_unregister_handler(webui->hs, webui->handler_redir);
	cs_destroy(&webui->parse);
	hdf_destroy(&webui->hdf);
	free(webui);
	dbg0("(webui) WebUI stopped");
}
