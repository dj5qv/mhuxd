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
#include <strings.h>
#include <ev.h>
#include <jansson.h>
#include "config.h"
#include "util.h"
#include "logger.h"
#include "devmgr.h"
#include "mhinfo.h"
#include "mhcontrol.h"
#include "wkman.h"
#include "mhsm.h"
#include "channel.h"
#include "conmgr.h"

#ifndef STATEDIR
#define STATEDIR "."
#endif

#define CFGFILE STATEDIR "/mhuxd-state.json"
#define MOD_ID "cfgmgrj"

struct cfgmgrj {
    struct ev_loop *loop;
    struct conmgr *conmgr;
    json_t *connectors;
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

struct json_connectors_builder {
    json_t *arr;
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

static int json_has_key(json_t *obj, const char *key) {
    if(!obj || !json_is_object(obj))
        return 0;
    return json_object_get(obj, key) != NULL;
}

struct ptt_map_entry {
    uint16_t keyer_type;
    const char *ptt_str;
    unsigned ptt1 : 1;
    unsigned ptt2 : 1;
    unsigned qsk : 1;
};

static const struct ptt_map_entry ptt_map[] = {
    { MHT_MK2, "ptt1", 1, 0, 0 },
    { MHT_MK2, "ptt2", 0, 1, 0 },
    { MHT_MK2, "ptt12", 1, 1, 0 },
    { MHT_MK2, "semi", 0, 0, 0 },
    { MHT_MK2, "qsk", 0, 0, 1 },

    { MHT_MK3, "ptt1", 1, 0, 0 },
    { MHT_MK3, "ptt2", 0, 1, 0 },
    { MHT_MK3, "ptt12", 1, 1, 0 },
    { MHT_MK3, "semi", 0, 0, 0 },
    { MHT_MK3, "qsk", 0, 0, 1 },

    { MHT_MK, "ptt1", 1, 0, 0 },
    { MHT_MK, "ptt2", 0, 1, 0 },
    { MHT_MK, "ptt12", 1, 1, 0 },
    { MHT_MK, "qsk", 0, 0, 1 },

    { MHT_DK, "ptt", 0, 1, 0 },
    { MHT_DK, "semi", 0, 0, 0 },
    { MHT_DK, "qsk", 0, 0, 1 },

    { MHT_DK2, "ptt", 0, 1, 0 },
    { MHT_DK2, "semi", 0, 0, 0 },
    { MHT_DK2, "qsk", 0, 0, 1 },

    { MHT_CK, "ptt", 0, 1, 0 },
    { MHT_CK, "qsk", 0, 0, 1 },
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
    { MHT_MK2Rp, "qsk", 0, 0, 1 }
};

static const char *compose_ptt_str(int type, int ptt1, int ptt2, int qsk) {
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
    case MHT_MK3:
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
        return "_invalid_";
    }
}

static int apply_ptt_value(struct mh_control *ctl, int type, const char *chan, const char *mode, const char *val) {
    if(!ctl || !chan || !mode || !val)
        return -1;

    const char *suffix = NULL;
    if(!strcmp(mode, "cw"))
        suffix = "FrBase_Cw";
    else if(!strcmp(mode, "voice"))
        suffix = "FrBase_Voice";
    else if(!strcmp(mode, "digital"))
        suffix = "FrBase_Digital";
    else
        return -1;

    const struct ptt_map_entry *match = NULL;
    for(size_t i = 0; i < ARRAY_SIZE(ptt_map); i++) {
        if(ptt_map[i].keyer_type == type && strcmp(ptt_map[i].ptt_str, val) == 0) {
            match = &ptt_map[i];
            break;
        }
    }
    if(!match) {
        warn("cfgmgrj: invalid ptt value '%s' for type %d", val, type);
        return -1;
    }

    char base[48];
    char key1[64];
    char key2[64];
    snprintf(base, sizeof(base), "%s%s", chan, suffix);
    snprintf(key1, sizeof(key1), "%s.ptt1", base);
    snprintf(key2, sizeof(key2), "%s.ptt2", base);

    if(mhc_set_kopt(ctl, key1, match->ptt1))
        return -1;
    if(mhc_set_kopt(ctl, key2, match->ptt2))
        return -1;

    if(!strcmp(mode, "cw")) {
        char qsk_key[32];
        snprintf(qsk_key, sizeof(qsk_key), "%sQsk", chan);
        if(mhc_set_kopt(ctl, qsk_key, match->qsk))
            return -1;
    }

    return 0;
}

static int apply_ptt_channel_from_json(struct mh_control *ctl, int type, const char *chan, json_t *chan_obj, int *changed) {
    if(!chan_obj || !json_is_object(chan_obj))
        return 0;

    const char *modes[] = { "cw", "voice", "digital" };
    for(size_t i = 0; i < ARRAY_SIZE(modes); i++) {
        json_t *val = json_object_get(chan_obj, modes[i]);
        if(!val)
            continue;
        if(!json_is_string(val))
            return -1;
        if(apply_ptt_value(ctl, type, chan, modes[i], json_string_value(val)) != 0)
            return -1;
        if(changed)
            *changed = 1;
    }

    return 0;
}

static int apply_ptt_from_json(struct mh_control *ctl, int type, json_t *ptt_obj, int *changed) {
    if(!ptt_obj || !json_is_object(ptt_obj))
        return 0;

    json_t *r1 = json_object_get(ptt_obj, "r1");
    if(apply_ptt_channel_from_json(ctl, type, "r1", r1, changed) != 0)
        return -1;

    json_t *r2 = json_object_get(ptt_obj, "r2");
    if(apply_ptt_channel_from_json(ctl, type, "r2", r2, changed) != 0)
        return -1;

    return 0;
}

static int param_get_int(json_t *param, const char *key, int defval) {
    if(!param || !json_is_object(param))
        return defval;
    return json_get_int(param, key, defval);
}

static int ptt_mode_supported(int type) {
    return !(type == MHT_DK || type == MHT_DK2 || type == MHT_CK);
}

static json_t *build_ptt_channel_json(int type, json_t *param, const char *chan) {
    if(!param || !chan)
        return NULL;

    char key1[64];
    char key2[64];
    char qsk_key[32];
    snprintf(key1, sizeof(key1), "%sFrBase_Cw.ptt1", chan);
    snprintf(key2, sizeof(key2), "%sFrBase_Cw.ptt2", chan);
    snprintf(qsk_key, sizeof(qsk_key), "%sQsk", chan);

    if(!json_has_key(param, key1) && !json_has_key(param, key2) && !json_has_key(param, qsk_key))
        return NULL;

    int ptt1 = param_get_int(param, key1, 0);
    int ptt2 = param_get_int(param, key2, 0);
    int qsk = param_get_int(param, qsk_key, 0);

    json_t *obj = json_object();
    if(!obj)
        return NULL;
    json_object_set_new(obj, "cw", json_string(compose_ptt_str(type, ptt1, ptt2, qsk)));

    if(ptt_mode_supported(type)) {
        snprintf(key1, sizeof(key1), "%sFrBase_Voice.ptt1", chan);
        snprintf(key2, sizeof(key2), "%sFrBase_Voice.ptt2", chan);
        ptt1 = param_get_int(param, key1, 0);
        ptt2 = param_get_int(param, key2, 0);
        json_object_set_new(obj, "voice", json_string(compose_ptt_str(type, ptt1, ptt2, qsk)));

        snprintf(key1, sizeof(key1), "%sFrBase_Digital.ptt1", chan);
        snprintf(key2, sizeof(key2), "%sFrBase_Digital.ptt2", chan);
        ptt1 = param_get_int(param, key1, 0);
        ptt2 = param_get_int(param, key2, 0);
        json_object_set_new(obj, "digital", json_string(compose_ptt_str(type, ptt1, ptt2, qsk)));
    }

    return obj;
}

static json_t *build_ptt_meta_json(int type, json_t *param) {
    json_t *ptt = json_object();
    if(!ptt)
        return NULL;

    json_t *r1 = build_ptt_channel_json(type, param, "r1");
    if(r1)
        json_object_set_new(ptt, "r1", r1);

    json_t *r2 = build_ptt_channel_json(type, param, "r2");
    if(r2)
        json_object_set_new(ptt, "r2", r2);

    if(json_object_size(ptt) == 0) {
        json_decref(ptt);
        return NULL;
    }

    return ptt;
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
        mhc_set_speed_params(ctl, channel, &cfg, completion_cb, &result);
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
            mhc_store_cw_message(ctl, (uint8_t)index, text, (uint8_t)next_idx, (uint8_t)delay, completion_cb, &result);
        } else {
            mhc_store_fsk_message(ctl, (uint8_t)index, text, (uint8_t)next_idx, (uint8_t)delay, completion_cb, &result);
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

/* Recursively apply sm fixed options from a JSON object, building dotted key paths
 * for nested objects (e.g. { "sequencer": { "lead": { "B6": 100 } } }
 * becomes sm_antsw_set_opt(sm, "sequencer.lead.B6", 100) */
static int apply_sm_fixed_recursive(struct sm *sm, json_t *obj, char *prefix, size_t prefix_len, size_t prefix_cap) {
    const char *key;
    json_t *val;
    json_object_foreach(obj, key, val) {
        size_t key_len = strlen(key);
        size_t new_len = prefix_len + (prefix_len ? 1 : 0) + key_len;
        if(new_len >= prefix_cap)
            continue;

        char *p = prefix + prefix_len;
        if(prefix_len) {
            *p++ = '.';
        }
        memcpy(p, key, key_len + 1);

        if(json_is_object(val)) {
            if(apply_sm_fixed_recursive(sm, val, prefix, new_len, prefix_cap))
                return -1;
        } else if(json_is_integer(val) || json_is_boolean(val) || json_is_real(val)) {
            int ival = json_is_real(val) ? (int)json_real_value(val) : (int)json_integer_value(val);
            if(json_is_boolean(val))
                ival = json_is_true(val) ? 1 : 0;
            if(sm_antsw_set_opt(sm, prefix, (uint32_t)ival))
                return -1;
        }

        /* restore prefix */
        prefix[prefix_len] = '\0';
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
        char prefix[256] = "";
        if(apply_sm_fixed_recursive(sm, fixed, prefix, 0, sizeof(prefix)))
            return -1;
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

    if(json_object_get(sm_obj, "obj")) {
        json_t *obj_val = json_object_get(sm_obj, "obj");
        if(json_is_array(obj_val)) {
            /* Bulk load from saved config: array of objects, each added */
            sm_antsw_clear_lists(sm);
            size_t idx;
            json_t *item;
            json_array_foreach(obj_val, idx, item) {
                if(!json_is_object(item))
                    continue;
                if(sm_antsw_add_obj_json(sm, item))
                    return -1;
            }
        } else if(json_is_object(obj_val)) {
            /* Single object operation: { "action": "add|mod|rem", ... } */
            const char *act = NULL;
            json_t *jact = json_object_get(obj_val, "action");
            if(jact && json_is_string(jact))
                act = json_string_value(jact);

            if(!act) {
                warn("sm.obj missing action field");
            } else if(strcmp(act, "add") == 0) {
                if(sm_antsw_add_obj_json(sm, obj_val))
                    return -1;
            } else if(strcmp(act, "mod") == 0) {
                if(sm_antsw_mod_obj_json(sm, obj_val))
                    return -1;
            } else if(strcmp(act, "rem") == 0) {
                int id = -1;
                json_t *jid = json_object_get(obj_val, "id");
                if(jid && json_is_integer(jid))
                    id = (int)json_integer_value(jid);
                if(id < 0) {
                    warn("sm.obj rem: missing id");
                    return -1;
                }
                if(sm_antsw_rem_obj(sm, id))
                    return -1;
            } else if(strcmp(act, "rem_ref") == 0) {
                int obj_id = -1, ref_id = -1;
                json_t *joid = json_object_get(obj_val, "obj_id");
                json_t *jrid = json_object_get(obj_val, "ref_id");
                if(joid && json_is_integer(joid)) obj_id = (int)json_integer_value(joid);
                if(jrid && json_is_integer(jrid)) ref_id = (int)json_integer_value(jrid);
                if(obj_id < 0 || ref_id < 0) {
                    warn("sm.obj rem_ref: missing obj_id or ref_id");
                    return -1;
                }
                if(sm_antsw_rem_obj_ref(sm, obj_id, ref_id))
                    return -1;
            } else {
                warn("sm.obj unknown action '%s'", act);
            }
        }
    }

    return 0;
}

static int apply_device_from_json(struct cfgmgrj *cfgmgrj, json_t *device_obj) {
    dbg1("%s", __func__);
    dbg1_j("device object", "", device_obj);

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

    dbg1_j("apply device ", serial, device_obj);

    struct mh_control *ctl = dev->ctl;
    if(type == 0)
        type = mhc_get_type(ctl);

    int ptt_changed = 0;
    json_t *ptt = json_object_get(device_obj, "ptt");
    if(ptt) {
        if(apply_ptt_from_json(ctl, type, ptt, &ptt_changed))
            return -1;
    }

    json_t *param = json_object_get(device_obj, "param");
    if(json_is_object(param)) {
        if(apply_kopts_from_json(ctl, param, ""))
            return -1;
        if(mhc_is_online(ctl)) {
            int result = -1;
            mhc_load_kopts(ctl, completion_cb, &result);
            while(result == -1)
                ev_loop(cfgmgrj->loop, EVRUN_ONCE);
            if(result != CMD_RESULT_OK)
                return -1;
        }
    }

    if(ptt_changed && (!param || !json_is_object(param))) {
        if(mhc_is_online(ctl)) {
            int result = -1;
            mhc_load_kopts(ctl, completion_cb, &result);
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

    dbg1("%s done", __func__);
    return 0;
}

static int apply_connector_from_json(struct cfgmgrj *cfgmgrj, json_t *conn_obj) {
    if(!cfgmgrj || !cfgmgrj->conmgr || !json_is_object(conn_obj))
        return -1;

    json_t *serial_val = json_object_get(conn_obj, "serial");
    json_t *channel_val = json_object_get(conn_obj, "channel");
    json_t *type_val = json_object_get(conn_obj, "type");
    if(!serial_val || !json_is_string(serial_val) || !channel_val || !json_is_string(channel_val) ||
       !type_val || !json_is_string(type_val))
        return -1;

    const char *serial = json_string_value(serial_val);
    const char *channel_str = json_string_value(channel_val);
    const char *type_str = json_string_value(type_val);
    int channel = ch_str2channel(channel_str);
    if(channel < 0)
        return -1;

    struct con_cfg ccfg = { 0 };
    ccfg.serial = serial;
    ccfg.channel = channel;
    ccfg.type = CON_INVALID;

    if(!strcasecmp(type_str, "VSP"))
        ccfg.type = CON_VSP;
    else if(!strcasecmp(type_str, "TCP"))
        ccfg.type = CON_TCP;

    if(ccfg.type == CON_VSP) {
        json_t *devname_val = json_object_get(conn_obj, "devname");
        if(!devname_val || !json_is_string(devname_val))
            return -1;
        ccfg.vsp.devname = json_string_value(devname_val);
        ccfg.vsp.maxcon = json_get_int(conn_obj, "maxcon", 1);
        ccfg.vsp.ptt_rts = json_get_int(conn_obj, "ptt_rts", 0);
        ccfg.vsp.ptt_dtr = json_get_int(conn_obj, "ptt_dtr", 0);
    } else if(ccfg.type == CON_TCP) {
        json_t *devname_val = json_object_get(conn_obj, "devname");
        if(!devname_val || !json_is_string(devname_val))
            return -1;
        ccfg.tcp.port = json_string_value(devname_val);
        ccfg.tcp.maxcon = json_get_int(conn_obj, "maxcon", 1);
        ccfg.tcp.remote_access = json_get_int(conn_obj, "remote_access", 0);
    } else {
        return -1;
    }

    int id = json_get_int(conn_obj, "id", 0);
    int run_id = conmgr_create_con_cfg(cfgmgrj->conmgr, cfgmgrj->loop, &ccfg, id);

    // Registry of Intent: update our internal list regardless of creation success
    if(cfgmgrj->connectors) {
        // If we don't have an ID yet but conmgr produced one (even if it failed, 
        // conmgr_create_con_cfg should ideally give us the ID it tried to use).
        // Since it returns 0 on failure, we'll use a fallback if id was 0.
        if (id <= 0 && run_id > 0) id = run_id;
        
        // If it still is 0 (failed and no ID provided), we need to find a unique one
        // to keep it in the intent list.
        if (id <= 0) {
            // This is a bit of a corner case for a new port that fails immediately.
            // Let's just find the max ID in our list and increment.
            int max_id = 0;
            size_t j;
            json_t *tmp;
            json_array_foreach(cfgmgrj->connectors, j, tmp) {
                int tid = json_get_int(tmp, "id", 0);
                if (tid > max_id) max_id = tid;
            }
            id = max_id + 1;
        }

        json_t *match = NULL;
        size_t idx;
        json_t *c;
        json_array_foreach(cfgmgrj->connectors, idx, c) {
            if(json_get_int(c, "id", 0) == id) {
                match = c;
                break;
            }
        }
        if(match) {
            json_array_set(cfgmgrj->connectors, idx, conn_obj);
            json_object_set_new(conn_obj, "id", json_integer(id));
        } else {
            json_t *new_obj = json_deep_copy(conn_obj);
            json_object_set_new(new_obj, "id", json_integer(id));
            json_array_append_new(cfgmgrj->connectors, new_obj);
        }
    }

    return 0;
}

static void connectors_builder_cb(const struct con_info *info, void *user_data) {
    struct json_connectors_builder *b = user_data;
    if(!b || !b->arr || !info)
        return;

    if(!info->serial || !info->devname)
        return;

    const char *type_str = NULL;
    if(info->type == CON_VSP)
        type_str = "VSP";
    else if(info->type == CON_TCP)
        type_str = "TCP";

    if(!type_str)
        return;

    json_t *obj = json_object();
    if(!obj)
        return;

    json_object_set_new(obj, "serial", json_string(info->serial));
    json_object_set_new(obj, "channel", json_string(ch_channel2str(info->channel)));
    json_object_set_new(obj, "type", json_string(type_str));
    json_object_set_new(obj, "devname", json_string(info->devname));

    if(info->id > 0)
        json_object_set_new(obj, "id", json_integer(info->id));
    if(info->maxcon > 0)
        json_object_set_new(obj, "maxcon", json_integer(info->maxcon));

    if(info->type == CON_VSP) {
        json_object_set_new(obj, "ptt_rts", json_boolean(info->ptt_rts ? 1 : 0));
        json_object_set_new(obj, "ptt_dtr", json_boolean(info->ptt_dtr ? 1 : 0));
    } else if(info->type == CON_TCP) {
        json_object_set_new(obj, "remote_access", json_boolean(info->remote_access ? 1 : 0));
    }

    json_array_append_new(b->arr, obj);
}

static int apply_config_json(struct cfgmgrj *cfgmgrj, json_t *root) {
    if(!json_is_object(root))
        return -1;

    dbg1("%s daemon", __func__);
    json_t *daemon = json_object_get(root, "daemon");
    if(json_is_object(daemon)) {
        json_t *loglevel = json_object_get(daemon, "loglevel");
        if(loglevel && json_is_string(loglevel)) {
            if(log_set_level_by_str(json_string_value(loglevel)) == -1)
                return -1;
        }
    }

    dbg1("%s devices", __func__);
    json_t *devices = json_object_get(root, "devices");
    if(devices && json_is_array(devices)) {
        size_t idx;
        json_t *device;
        json_array_foreach(devices, idx, device) {
            if(apply_device_from_json(cfgmgrj, device))
                return -1;
        }
    }

    dbg1("%s connectors", __func__);
    json_t *connectors = json_object_get(root, "connectors");
    if(connectors && json_is_array(connectors)) {
        size_t idx;
        json_t *connector;
        json_array_foreach(connectors, idx, connector) {
            if(apply_connector_from_json(cfgmgrj, connector))
                return -1;
        }
    }

    dbg1("%s connectorsRemove", __func__);
    json_t *connectors_remove = json_object_get(root, "connectorsRemove");
    if(connectors_remove && json_is_array(connectors_remove)) {
        size_t idx;
        json_t *id_val;
        json_array_foreach(connectors_remove, idx, id_val) {
            if(!json_is_integer(id_val))
                return -1;
            int id = (int)json_integer_value(id_val);
            conmgr_destroy_con(cfgmgrj->conmgr, id);

            // Registry of Intent: remove from our list
            if(cfgmgrj->connectors) {
                size_t j;
                json_t *c;
                json_array_foreach(cfgmgrj->connectors, j, c) {
                    if(json_get_int(c, "id", 0) == id) {
                        json_array_remove(cfgmgrj->connectors, j);
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

int cfgmgrj_apply_json(struct cfgmgrj *cfgmgrj, json_t *root) {
    return apply_config_json(cfgmgrj, root);
}

static json_t *build_sm_json(struct device *dev) {
    struct sm *sm = mhc_get_sm(dev->ctl);
    if(!sm)
        return NULL;
    return sm_antsw_to_json(sm);
}

static json_t *build_config_json(struct cfgmgrj *cfgmgrj) {
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
                    json_t *ptt = build_ptt_meta_json((int)mhc_get_type(dev->ctl), param);
                    if(ptt)
                        json_object_set_new(device, "ptt", ptt);
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

            json_t *sm_json = build_sm_json(dev);
            if(sm_json)
                json_object_set_new(device, "sm", sm_json);

            json_array_append_new(devices, device);
        }
    }

    json_object_set_new(root, "devices", devices);
    if(cfgmgrj && cfgmgrj->connectors && json_is_array(cfgmgrj->connectors)) {
        json_object_set_new(root, "connectors", json_incref(cfgmgrj->connectors));
    }
    return root;
}

// Build the complete JSON config.
json_t *cfgmgrj_build_json(struct cfgmgrj *cfgmgrj) {
    json_t *root = build_config_json(cfgmgrj);
    if(!root) return NULL;

    // Decorate connectors with runtime status for the UI.
    // We deep copy the connectors list so we don't save runtime status to the config file.
    json_t *connectors = json_object_get(root, "connectors");
    if(connectors && json_is_array(connectors) && cfgmgrj->conmgr) {
        json_t *copy = json_deep_copy(connectors);
        json_object_set_new(root, "connectors", copy);
        connectors = copy;

        size_t idx;
        json_t *c;
        json_array_foreach(connectors, idx, c) {
            int id = json_get_int(c, "id", 0);
            int running = conmgr_exists(cfgmgrj->conmgr, id);
            json_object_set_new(c, "status", json_string(running ? "ok" : "failed"));
        }
    }

    return root;
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

int cfgmgrj_sm_load(const char *serial) {
    dbg1("%s()", __func__);
    struct device *dev = dmgr_get_device(serial);
    struct sm *sm;

    if(!dev) {
        err("%s keyer not found!", serial);
        return -1;
    }

    sm = mhc_get_sm(dev->ctl);

    if(!sm) {
        err("%s no SM structure found!", serial);
        return -1;
    }

    if(0 != sm_get_antsw(sm)) {
        err("%s could not load antsw settings!", serial);
        return -1;
    }
    return 0;
}

int cfgmgrj_sm_store(const char *serial) {
    dbg1("%s()", __func__);
    struct device *dev = dmgr_get_device(serial);
    struct sm *sm;

    if(!dev) {
        err("%s keyer not found!", serial);
        return -1;
    }

    sm = mhc_get_sm(dev->ctl);

    if(!sm) {
        err("%s no SM structure found!", serial);
        return -1;
    }

    if(0 != sm_antsw_store(sm)) {
        err("%s could not store antsw settings!", serial);
        return -1;
    }

    return 0;
}

struct cfgmgrj *cfgmgrj_create(struct ev_loop *loop, struct conmgr *conmgr) {
    struct cfgmgrj *cfgmgrj = w_calloc(1, sizeof(*cfgmgrj));
    if(!cfgmgrj)
        return NULL;

    cfgmgrj->loop = loop;
    cfgmgrj->conmgr = conmgr;
    cfgmgrj->connectors = json_array();
    return cfgmgrj;
}

 void cfgmgrj_destroy(struct cfgmgrj *cfgmgrj) {
    if(!cfgmgrj)
        return;
    if(cfgmgrj->connectors)
        json_decref(cfgmgrj->connectors);

    free(cfgmgrj);
    return;
}

int cfgmgrj_load_cfg(struct cfgmgrj *cfgmgrj) {
    json_error_t jerr;
    json_t *root = json_load_file(CFGFILE, 0, &jerr);

    dbg1("%s", __func__);

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
    json_t *root = build_config_json(cfgmgrj);
    if(!root)
        return -1;

    if(json_dump_file(root, CFGFILE, JSON_INDENT(2) | JSON_PRESERVE_ORDER) != 0) {
        json_decref(root);
        return -1;
    }
    json_decref(root);
    return 0;
}