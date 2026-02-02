/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <ev.h>
#include <jansson.h>
#include "config.h"
#include "util.h"
#include "logger.h"
#include "devmgr.h"
#include "mhcontrol.h"
#include "wkman.h"
#include "mhsm.h"
#include "channel.h"

#ifndef STATEDIR
#define STATEDIR "."
#endif

#define CFGFILE STATEDIR "/mhuxd-state.json"
#define MOD_ID "cfgmgrj"

struct cfgmgrj {
    struct ev_loop *loop;
 };

static json_t *build_speed_obj(const struct mhc_speed_cfg *cfg);
static int param_builder_cb(const char *key, int val, void *user_data);
static int winkey_builder_cb(const char *key, int val, void *user_data);

struct json_param_builder {
    json_t *obj;
};

struct json_winkey_builder {
    json_t *obj;
};

static void completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data) {
    (void)reply_buf; (void)len;
    int *notify = user_data;
    *notify = result;
}

static int json_get_int(json_t *obj, const char *key, int defval) {
    json_t *val = json_object_get(obj, key);
    if(!val)
        return defval;
    if(json_is_integer(val))
        return (int)json_integer_value(val);
    if(json_is_boolean(val))
        return json_is_true(val) ? 1 : 0;
    if(json_is_real(val))
        return (int)json_real_value(val);
    return defval;
}

static double json_get_double(json_t *obj, const char *key, double defval) {
    json_t *val = json_object_get(obj, key);
    if(!val)
        return defval;
    if(json_is_real(val))
        return json_real_value(val);
    if(json_is_integer(val))
        return (double)json_integer_value(val);
    return defval;
}

static int apply_kopts_from_json(struct mh_control *ctl, json_t *param_obj, const char *prefix) {
    const char *key;
    json_t *val;
    json_object_foreach(param_obj, key, val) {
        char full_key[256];
        if(prefix && *prefix)
            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key);
        else
            snprintf(full_key, sizeof(full_key), "%s", key);

        if(json_is_object(val)) {
            if(apply_kopts_from_json(ctl, val, full_key))
                return -1;
            continue;
        }

        if(!json_is_integer(val) && !json_is_boolean(val) && !json_is_real(val))
            continue;
        int ival = json_is_real(val) ? (int)json_real_value(val) : (int)json_integer_value(val);
        if(json_is_boolean(val))
            ival = json_is_true(val) ? 1 : 0;
        if(mhc_set_kopt(ctl, full_key, ival))
            return -1;
    }
    return 0;
}

static int apply_speed_from_json(struct cfgmgrj *cfgmgrj, struct mh_control *ctl, json_t *channel_obj) {
    const char *chan_name;
    json_t *chan_cfg;
    json_object_foreach(channel_obj, chan_name, chan_cfg) {
        if(!json_is_object(chan_cfg))
            continue;
        int channel = ch_str2channel(chan_name);
        if(channel < 0 || channel >= MH_NUM_CHANNELS) {
            err("invalid channel '%s' in JSON config", chan_name);
            return -1;
        }

        struct mhc_speed_cfg cfg;
        cfg.baud = json_get_double(chan_cfg, "baud", 0);
        cfg.stopbits = json_get_double(chan_cfg, "stopbits", 1);
        cfg.databits = json_get_int(chan_cfg, "databits", 8);
        cfg.rtscts = json_get_int(chan_cfg, "rtscts", 0);
        cfg.rigtype = json_get_int(chan_cfg, "rigtype", 0);
        cfg.icomaddress = json_get_int(chan_cfg, "icomaddress", 0);
        cfg.icomsimulateautoinfo = json_get_int(chan_cfg, "icomsimulateautoinfo", 0);
        cfg.digitalovervoicerule = json_get_int(chan_cfg, "digitalovervoicerule", 0);
        cfg.usedecoderifconnected = json_get_int(chan_cfg, "usedecoderifconnected", 0);
        cfg.dontinterfereusbcontrol = json_get_int(chan_cfg, "dontinterfereusbcontrol", 0);

        int result = -1;
        if(mhc_set_speed_params(ctl, channel, &cfg, completion_cb, &result))
            return -1;
        while(result == -1)
            ev_loop(cfgmgrj->loop, EVRUN_ONCE);
        if(result != CMD_RESULT_OK)
            return -1;
    }
    return 0;
}

static int apply_messages_from_json(struct cfgmgrj *cfgmgrj, struct mh_control *ctl, json_t *arr, int is_cw) {
    if(!json_is_array(arr))
        return 0;
    size_t idx;
    json_t *msg;
    json_array_foreach(arr, idx, msg) {
        if(!json_is_object(msg))
            continue;
        int index = json_get_int(msg, "index", (int)idx + 1);
        json_t *text_val = json_object_get(msg, "text");
        if(!text_val || !json_is_string(text_val))
            continue;
        const char *text = json_string_value(text_val);
        int next_idx = json_get_int(msg, "nextIdx", 0xff);
        int delay = json_get_int(msg, "delay", 0);

        int result = -1;
        if(is_cw) {
            if(mhc_store_cw_message(ctl, (uint8_t)index, text, (uint8_t)next_idx, (uint8_t)delay, completion_cb, &result))
                return -1;
        } else {
            if(mhc_store_fsk_message(ctl, (uint8_t)index, text, (uint8_t)next_idx, (uint8_t)delay, completion_cb, &result))
                return -1;
        }
        while(result == -1)
            ev_loop(cfgmgrj->loop, EVRUN_ONCE);
        if(result != CMD_RESULT_OK)
            return -1;
    }
    return 0;
}

static int apply_winkey_from_json(struct cfgmgrj *cfgmgrj, struct device *dev, json_t *winkey_obj) {
    if(!json_is_object(winkey_obj))
        return 0;
    if(!dev->wkman)
        dev->wkman = wkm_create(cfgmgrj->loop, dev);
    if(!dev->wkman)
        return -1;

    const char *key;
    json_t *val;
    json_object_foreach(winkey_obj, key, val) {
        if(!json_is_integer(val) && !json_is_boolean(val) && !json_is_real(val))
            continue;
        int ival = json_is_real(val) ? (int)json_real_value(val) : (int)json_integer_value(val);
        if(json_is_boolean(val))
            ival = json_is_true(val) ? 1 : 0;
        if(wkm_set_value(dev->wkman, key, (uint8_t)ival))
            return -1;
    }

    if(mhc_is_online(dev->ctl)) {
        int werr = wkm_write_cfg(dev->wkman);
        if(WKM_RESULT_OK != werr) {
            err("could not write config to winkey (%s)!", wkm_err_string(werr));
            return -1;
        }
    }
    return 0;
}

static int apply_sm_from_json(struct device *dev, json_t *sm_obj) {
    if(!json_is_object(sm_obj))
        return 0;
    struct sm *sm = mhc_get_sm(dev->ctl);
    if(!sm)
        return 0;

    json_t *fixed = json_object_get(sm_obj, "fixed");
    if(json_is_object(fixed)) {
        const char *key;
        json_t *val;
        json_object_foreach(fixed, key, val) {
            if(!json_is_integer(val) && !json_is_boolean(val) && !json_is_real(val))
                continue;
            int ival = json_is_real(val) ? (int)json_real_value(val) : (int)json_integer_value(val);
            if(json_is_boolean(val))
                ival = json_is_true(val) ? 1 : 0;
            if(sm_antsw_set_opt(sm, key, (uint32_t)ival))
                return -1;
        }
    }

    json_t *output = json_object_get(sm_obj, "output");
    if(json_is_object(output)) {
        const char *key;
        json_t *val;
        json_object_foreach(output, key, val) {
            if(!json_is_integer(val) && !json_is_boolean(val) && !json_is_real(val))
                continue;
            int ival = json_is_real(val) ? (int)json_real_value(val) : (int)json_integer_value(val);
            if(json_is_boolean(val))
                ival = json_is_true(val) ? 1 : 0;
            if(sm_antsw_set_output(sm, key, (uint8_t)ival))
                return -1;
        }
    }

    if(json_object_get(sm_obj, "obj"))
        warn("sm.obj not supported in JSON config yet");

    return 0;
}

static int apply_device_from_json(struct cfgmgrj *cfgmgrj, json_t *device_obj) {
    if(!json_is_object(device_obj))
        return -1;

    json_t *serial_val = json_object_get(device_obj, "serial");
    if(!serial_val || !json_is_string(serial_val))
        return -1;
    const char *serial = json_string_value(serial_val);

    int type = json_get_int(device_obj, "type", 0);
    struct device *dev = dmgr_get_device(serial);
    if(!dev)
        dev = dmgr_add_device(serial, (uint16_t)type);
    if(!dev)
        return -1;

    struct mh_control *ctl = dev->ctl;

    json_t *param = json_object_get(device_obj, "param");
    if(json_is_object(param)) {
        if(apply_kopts_from_json(ctl, param, ""))
            return -1;
        if(mhc_is_online(ctl)) {
            int result = -1;
            if(mhc_load_kopts(ctl, completion_cb, &result))
                return -1;
            while(result == -1)
                ev_loop(cfgmgrj->loop, EVRUN_ONCE);
            if(result != CMD_RESULT_OK)
                return -1;
        }
    }

    json_t *channel = json_object_get(device_obj, "channel");
    if(json_is_object(channel)) {
        if(apply_speed_from_json(cfgmgrj, ctl, channel))
            return -1;
    }

    json_t *cw = json_object_get(device_obj, "cwMessages");
    if(cw) {
        if(apply_messages_from_json(cfgmgrj, ctl, cw, 1))
            return -1;
    }

    json_t *fsk = json_object_get(device_obj, "fskMessages");
    if(fsk) {
        if(apply_messages_from_json(cfgmgrj, ctl, fsk, 0))
            return -1;
    }

    json_t *winkey = json_object_get(device_obj, "winkey");
    if(winkey) {
        if(apply_winkey_from_json(cfgmgrj, dev, winkey))
            return -1;
    }

    json_t *sm = json_object_get(device_obj, "sm");
    if(sm) {
        if(apply_sm_from_json(dev, sm))
            return -1;
    }

    return 0;
}

static int apply_config_json(struct cfgmgrj *cfgmgrj, json_t *root) {
    if(!json_is_object(root))
        return -1;

    json_t *daemon = json_object_get(root, "daemon");
    if(json_is_object(daemon)) {
        json_t *loglevel = json_object_get(daemon, "loglevel");
        if(loglevel && json_is_string(loglevel)) {
            if(log_set_level_by_str(json_string_value(loglevel)) == -1)
                return -1;
        }
    }

    json_t *devices = json_object_get(root, "devices");
    if(devices && json_is_array(devices)) {
        size_t idx;
        json_t *device;
        json_array_foreach(devices, idx, device) {
            if(apply_device_from_json(cfgmgrj, device))
                return -1;
        }
    }

    return 0;
}

int cfgmgrj_apply_json(struct cfgmgrj *cfgmgrj, json_t *root) {
    return apply_config_json(cfgmgrj, root);
}

static json_t *build_config_json(void) {
    json_t *root = json_object();
    if(!root)
        return NULL;

    json_t *daemon = json_object();
    if(!daemon) {
        json_decref(root);
        return NULL;
    }
    json_object_set_new(daemon, "loglevel", json_string(log_get_level_str()));
    json_object_set_new(root, "daemon", daemon);

    json_t *devices = json_array();
    if(!devices) {
        json_decref(root);
        return NULL;
    }

    struct PGList *list = dmgr_get_device_list();
    if(list) {
        struct device *dev;
        PG_SCANLIST(list, dev) {
            json_t *device = json_object();
            if(!device) {
                json_decref(devices);
                json_decref(root);
                return NULL;
            }

            json_object_set_new(device, "serial", json_string(dev->serial ? dev->serial : ""));
            json_object_set_new(device, "type", json_integer(mhc_get_type(dev->ctl)));

            json_t *channel = json_object();
            if(channel) {
                for(int ch = 0; ch < MH_NUM_CHANNELS; ch++) {
                    struct mhc_speed_cfg cfg;
                    if(mhc_get_speed_params(dev->ctl, ch, &cfg) == 0) {
                        const char *ch_name = ch_channel2str(ch);
                        char lower[32];
                        size_t i;
                        for(i = 0; i < sizeof(lower) - 1 && ch_name[i]; i++)
                            lower[i] = (char)tolower((unsigned char)ch_name[i]);
                        lower[i] = 0;
                        json_t *cfg_obj = build_speed_obj(&cfg);
                        if(cfg_obj)
                            json_object_set_new(channel, lower, cfg_obj);
                    }
                }
                if(json_object_size(channel) > 0)
                    json_object_set_new(device, "channel", channel);
                else
                    json_decref(channel);
            }

            json_t *param = json_object();
            if(param) {
                struct json_param_builder pb = { .obj = param };
                if(mhc_kopts_foreach(dev->ctl, param_builder_cb, &pb) == 0) {
                    if(json_object_size(param) > 0)
                        json_object_set_new(device, "param", param);
                    else
                        json_decref(param);
                } else {
                    json_decref(param);
                }
            }

            json_t *cw_arr = json_array();
            json_t *fsk_arr = json_array();
            for(int i = 1; i <= 9; i++) {
                uint8_t next_idx, delay;
                const char *text = mhc_get_cw_message(dev->ctl, i, &next_idx, &delay);
                if(text && *text) {
                    json_t *msg = json_object();
                    json_object_set_new(msg, "index", json_integer(i));
                    json_object_set_new(msg, "text", json_string(text));
                    json_object_set_new(msg, "nextIdx", json_integer(next_idx));
                    json_object_set_new(msg, "delay", json_integer(delay));
                    json_array_append_new(cw_arr, msg);
                }

                text = mhc_get_fsk_message(dev->ctl, i, &next_idx, &delay);
                if(text && *text) {
                    json_t *msg = json_object();
                    json_object_set_new(msg, "index", json_integer(i));
                    json_object_set_new(msg, "text", json_string(text));
                    json_object_set_new(msg, "nextIdx", json_integer(next_idx));
                    json_object_set_new(msg, "delay", json_integer(delay));
                    json_array_append_new(fsk_arr, msg);
                }
            }
            if(json_array_size(cw_arr) > 0)
                json_object_set_new(device, "cwMessages", cw_arr);
            else
                json_decref(cw_arr);

            if(json_array_size(fsk_arr) > 0)
                json_object_set_new(device, "fskMessages", fsk_arr);
            else
                json_decref(fsk_arr);

            if(dev->wkman) {
                json_t *winkey = json_object();
                if(winkey) {
                    struct json_winkey_builder wb = { .obj = winkey };
                    if(wkm_foreach(dev->wkman, winkey_builder_cb, &wb) == 0) {
                        if(json_object_size(winkey) > 0)
                            json_object_set_new(device, "winkey", winkey);
                        else
                            json_decref(winkey);
                    } else {
                        json_decref(winkey);
                    }
                }
            }

            json_array_append_new(devices, device);
        }
    }

    json_object_set_new(root, "devices", devices);
    return root;
}

json_t *cfgmgrj_build_json(struct cfgmgrj *cfgmgrj) {
    (void)cfgmgrj;
    return build_config_json();
}

static json_t *build_speed_obj(const struct mhc_speed_cfg *cfg) {
    json_t *obj = json_object();
    if(!obj)
        return NULL;

    json_object_set_new(obj, "baud", json_real(cfg->baud));
    json_object_set_new(obj, "stopbits", json_real(cfg->stopbits));
    json_object_set_new(obj, "databits", json_integer(cfg->databits));
    json_object_set_new(obj, "rtscts", json_integer(cfg->rtscts));
    if(cfg->rigtype)
        json_object_set_new(obj, "rigtype", json_integer(cfg->rigtype));
    if(cfg->icomaddress)
        json_object_set_new(obj, "icomaddress", json_integer(cfg->icomaddress));
    if(cfg->icomsimulateautoinfo)
        json_object_set_new(obj, "icomsimulateautoinfo", json_integer(cfg->icomsimulateautoinfo));
    if(cfg->digitalovervoicerule)
        json_object_set_new(obj, "digitalovervoicerule", json_integer(cfg->digitalovervoicerule));
    if(cfg->usedecoderifconnected)
        json_object_set_new(obj, "usedecoderifconnected", json_integer(cfg->usedecoderifconnected));
    if(cfg->dontinterfereusbcontrol)
        json_object_set_new(obj, "dontinterfereusbcontrol", json_integer(cfg->dontinterfereusbcontrol));
    return obj;
}

static int param_builder_cb(const char *key, int val, void *user_data) {
    struct json_param_builder *pb = user_data;
    if(!pb || !pb->obj)
        return -1;
    if(json_object_set_new(pb->obj, key, json_integer(val)) != 0)
        return -1;
    return 0;
}

static int winkey_builder_cb(const char *key, int val, void *user_data) {
    struct json_winkey_builder *wb = user_data;
    if(!wb || !wb->obj)
        return -1;
    if(json_object_set_new(wb->obj, key, json_integer(val)) != 0)
        return -1;
    return 0;
}

struct cfgmgrj *cfgmgrj_create(struct ev_loop *loop) {
    struct cfgmgrj *cfgmgrj = w_calloc(1, sizeof(*cfgmgrj));
    if(!cfgmgrj)
        return NULL;

    cfgmgrj->loop = loop;
    return cfgmgrj;
}

 void cfgmgrj_destroy(struct cfgmgrj *cfgmgrj) {
    if(!cfgmgrj)
        return;

    free(cfgmgrj);
    return;
}

int cfgmgrj_load_cfg(struct cfgmgrj *cfgmgrj) {
    json_error_t jerr;
    json_t *root = json_load_file(CFGFILE, 0, &jerr);
    if(!root) {
        if(jerr.line == 0)
            return 0;
        err("cfgmgrj: failed to read %s: %s", CFGFILE, jerr.text);
        return -1;
    }

    int rc = apply_config_json(cfgmgrj, root);
    json_decref(root);
    return rc;
}

int cfgmgrj_save_cfg(struct cfgmgrj *cfgmgrj) {
    json_t *root = build_config_json();
    if(!root)
        return -1;

    if(json_dump_file(root, CFGFILE, JSON_INDENT(2) | JSON_PRESERVE_ORDER) != 0) {
        json_decref(root);
        return -1;
    }
    json_decref(root);
    return 0;
}