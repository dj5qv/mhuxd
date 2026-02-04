/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2017  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <ev.h>
#include "clearsilver/util/neo_hdf.h"
#include "cfgmgr.h"
#include "logger.h"
#include "util.h"
#include "cfgnod.h"
#include "version.h"
#include "devmgr.h"
#include "mhcontrol.h"
#include "mhinfo.h"
#include "conmgr.h"
#include "radiotypes.h"
#include "channel.h"
#include "wkman.h"
#include "mhsm.h"

#define MOD_ID "cfgmgr"

#if !defined EV_VERSION_MAJOR || EV_VERSION_MAJOR < 4
#define EVRUN_ONCE EVLOOP_ONESHOT
#endif

#ifndef STATEDIR
#error STATEDIR not defined
#endif

#define CFGFILE STATEDIR "/mhuxd-state.hdf"
#define MAX_HDF_PATH_LEN (128)

extern const char *log_file_name;

struct cfgmgr {
	struct ev_loop *loop;
	struct conmgr *conmgr;
	struct cfg *runtime_cfg;
};

static void log_neoerr(NEOERR *err, const char *what) {
	STRING str;

	string_init(&str);

	nerr_error_string(err, &str);
	err("%s(%s)", what, str.buf ? str.buf : "Unkown");
	string_clear(&str);
	nerr_ignore(&err);
}

static int merge_runtime_cfg(struct cfg *cfg) {
	int rval = 0;
	char buf[HOST_NAME_MAX + 1];
	gethostname(buf, HOST_NAME_MAX);

	rval |= cfg_set_value(cfg, "mhuxd.run.program.name", _package);
	rval |= cfg_set_value(cfg, "mhuxd.run.program.version", _package_version);
	rval |= cfg_set_value(cfg, "mhuxd.run.hostname", buf);
	rval |= cfg_set_value(cfg, "mhuxd.run.logfile", log_file_name);
	rval |= cfg_set_int_value(cfg, "mhuxd.run.pid", getpid());

	int i;
	for(i = 0; i < num_rig_types; i++) {
		char buf[128];
		snprintf(buf, sizeof(buf)-1, "mhuxd.run.rigtype.%d.name", rig_types[i].key);
		rval |= cfg_set_value(cfg, buf, rig_types[i].name);
		snprintf(buf, sizeof(buf)-1, "mhuxd.run.rigtype.%d.icom_addr", rig_types[i].key);
		rval |= cfg_set_int_value(cfg, buf, rig_types[i].icom_addr);
	}
	return rval;
}

enum {
        MOD_CW,
        MOD_VOICE,
	MOD_FSK,
        MOD_DIGITAL,
};

static int mk1_set_frbase(struct device *dev, struct cfg *param_cfg) {
	int rval = 0;

	// Ugly special treatment for MK1
	const struct mh_info *mhi;
	mhi = mhc_get_mhinfo(dev->ctl);
	if(mhi && mhi->type == MHT_MK ) {
		HDF *param_hdf = (HDF *)param_cfg;
		int mode, audioRx = -1, audioTx = -1, audioTxFootSw = -1, ptt1 = -1, ptt2 = -1;

		dbg1("%s()", __func__);

		mode = hdf_get_int_value(param_hdf, "r1KeyerMode", -1);
		switch(mode) {
		case MOD_CW:
			audioRx = hdf_get_int_value(param_hdf, "r1FrBase_Cw.audioRx", -1);
			audioTx = hdf_get_int_value(param_hdf, "r1FrBase_Cw.audioTx", -1);
			audioTxFootSw = hdf_get_int_value(param_hdf, "r1FrBase_Cw.audioTxFootSw", -1);
			ptt1 = hdf_get_int_value(param_hdf, "r1FrBase_Cw.ptt1", -1);
			ptt2 = hdf_get_int_value(param_hdf, "r1FrBase_Cw.ptt2", -1);
			break;
		case MOD_VOICE:
			audioRx = hdf_get_int_value(param_hdf, "r1FrBase_Voice.audioRx", -1);
			audioTx = hdf_get_int_value(param_hdf, "r1FrBase_Voice.audioTx", -1);
			audioTxFootSw = hdf_get_int_value(param_hdf, "r1FrBase_Voice.audioTxFootSw", -1);
			ptt1 = hdf_get_int_value(param_hdf, "r1FrBase_Voice.ptt1", -1);
			ptt2 = hdf_get_int_value(param_hdf, "r1FrBase_Voice.ptt2", -1);
			break;
		case MOD_FSK:
		case MOD_DIGITAL:
			audioRx = hdf_get_int_value(param_hdf, "r1FrBase_Digital.audioRx", -1);
			audioTx = hdf_get_int_value(param_hdf, "r1FrBase_Digital.audioTx", -1);
			audioTxFootSw = hdf_get_int_value(param_hdf, "r1FrBase_Digital.audioTxFootSw", -1);
			ptt1 = hdf_get_int_value(param_hdf, "r1FrBase_Digital.ptt1", -1);
			ptt2 = hdf_get_int_value(param_hdf, "r1FrBase_Digital.ptt2", -1);
			break;
		}

		if(audioRx >= 0)
			rval += mhc_set_kopt(dev->ctl, "r1FrBase.audioRx", audioRx);
		if(audioTx >= 0)
			rval += mhc_set_kopt(dev->ctl, "r1FrBase.audioTx", audioTx);
		if(audioTxFootSw >= 0)
			rval += mhc_set_kopt(dev->ctl, "r1FrBase.audioTxFootSw", audioTxFootSw);
		if(ptt1 >= 0)
			rval += mhc_set_kopt(dev->ctl, "r1FrBase.ptt1", ptt1);
		if(ptt2 >= 0)
			rval += mhc_set_kopt(dev->ctl, "r1FrBase.ptt2", ptt2);
	}

	return rval;
}

//FIXME: remove this
void cfgmr_state_changed_cb(const char *serial, int state, void *user_data) {
	(void)state; (void)serial; (void)user_data;
	//struct cfgmgr *cfgmgr = user_data;
	dbg1("%s() %s", __func__, mhc_state_str(state));
	//cfgmgr_update_hdf_dev(cfgmgr, serial);
}

// Merge device configuration into cfg
int merge_device_cfg(struct cfgmgr *cfgmgr, struct device *dev, struct cfg *cfg) {
	NEOERR *err;
	HDF *hdf = (HDF *)cfg;
	char full_key[MAX_HDF_PATH_LEN + 1];
	const struct mh_info *mhi;
	const char *serial = dev->serial;

	dbg1("%s()", __func__);

	if(!dev || !hdf)
		return -1;

	mhi = mhc_get_mhinfo(dev->ctl);

	// Keyer node
	HDF *knod, *param_nod, *flags_nod, *chan_nod, *run_nod, *winkey_nod, *cw_messages_nod, *fsk_messages_nod;

	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.keyer.%s", serial);
	err = hdf_get_node(hdf, full_key, &knod);
	if(err != STATUS_OK) goto failed;

	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.run.keyer.%s", serial);
	err = hdf_get_node(hdf, full_key, &run_nod);
	if(err != STATUS_OK) goto failed;

	// Keyer status
	err = hdf_set_value(run_nod, "info.status", mhc_state_str(mhc_get_state((dev->ctl))));
	if(err != STATUS_OK) goto failed;


	// Keyer type
	err = hdf_set_int_value(knod, "type", mhi->type);
	if(err != STATUS_OK) goto failed;

	// Keyer info

	err = hdf_set_int_value(run_nod, "info.type", mhi->type);
	if(err != STATUS_OK) goto failed;

	err = hdf_set_value(run_nod, "info.name", mhi->type_str);
	if(err != STATUS_OK) goto failed;

	err = hdf_set_int_value(run_nod, "info.ver_fw_major", mhi->ver_fw_major);
	if(err != STATUS_OK) goto failed;

	err = hdf_set_int_value(run_nod, "info.ver_fw_minor", mhi->ver_fw_minor);
	if(err != STATUS_OK) goto failed;

	err = hdf_set_int_value(run_nod, "info.ver_winkey", mhi->ver_winkey);
	if(err != STATUS_OK) goto failed;


	// Keyer flags

	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.run.keyer.%s.flags", serial);
	err = hdf_get_node(hdf, full_key, &flags_nod);
	if(err != STATUS_OK) goto failed;

	err = hdf_set_int_value(flags_nod, "has.r1", mhi->flags & MHF_HAS_R1 ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.r2", mhi->flags & MHF_HAS_R2 ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.r1.radio_support", mhi->flags & MHF_HAS_R1_RADIO_SUPPORT ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.r2.radio_support", mhi->flags & MHF_HAS_R2_RADIO_SUPPORT ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.aux", mhi->flags & MHF_HAS_AUX ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.winkey", mhi->flags & MHF_HAS_WINKEY ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.fsk1", mhi->flags & MHF_HAS_FSK1 ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.fsk2", mhi->flags & MHF_HAS_FSK2 ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.frbase", mhi->flags & MHF_HAS_FRBASE ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.frbase_cw", mhi->flags & MHF_HAS_FRBASE_CW ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.frbase_digital", mhi->flags & MHF_HAS_FRBASE_DIGITAL ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.frbase_voice", mhi->flags & MHF_HAS_FRBASE_VOICE ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.lna_pa_ptt", mhi->flags & MHF_HAS_LNA_PA_PTT ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.lna_pa_ptt_tail", mhi->flags & MHF_HAS_LNA_PA_PTT_TAIL ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.soundcard_ptt", mhi->flags & MHF_HAS_SOUNDCARD_PTT ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.cw_in_voice", mhi->flags & MHF_HAS_CW_IN_VOICE ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.audio_switching", mhi->flags & MHF_HAS_AUDIO_SWITCHING ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.display", mhi->flags & MHF_HAS_DISPLAY ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.follow_tx_mode", mhi->flags & MHF_HAS_FOLLOW_TX_MODE ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.ptt_settings", mhi->flags & MHF_HAS_PTT_SETTINGS ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.keyer_mode", mhi->flags & MHF_HAS_KEYER_MODE ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.flags_channel", mhi->flags & MHF_HAS_FLAGS_CHANNEL ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.mcp_support", mhi->flags & MHF_HAS_MCP_SUPPORT ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.rotator_support", mhi->flags & MHF_HAS_ROTATOR_SUPPORT ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.sm_commands", mhi->flags & MHF_HAS_SM_COMMANDS ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.pfsk", mhi->flags & MHF_HAS_PFSK ? 1 : 0);
	if(err != STATUS_OK) goto failed;
	err = hdf_set_int_value(flags_nod, "has.pcw", mhi->flags & MHF_HAS_PCW ? 1 : 0);
	if(err != STATUS_OK) goto failed;

	// Keyer channels
	// FIXME: move this logic into mhuxd.hdf,
	//        we already have corresponding "has." flags.
	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.run.keyer.%s.channels", serial);
	err = hdf_get_node(hdf, full_key, &chan_nod);
	if(err != STATUS_OK) goto failed;

	if(mhi->flags & MHF_HAS_R1) {
		err = hdf_set_int_value(chan_nod, "CAT1", 1);
		if(err != STATUS_OK) goto failed;
	}
	if(mhi->flags & MHF_HAS_R2) {
		err = hdf_set_int_value(chan_nod, "CAT2", 1);
		if(err != STATUS_OK) goto failed;
	}

	if(mhi->flags & MHF_HAS_FLAGS_CHANNEL) {
		err = hdf_set_int_value(chan_nod, "PTT1", 1);
		if(err != STATUS_OK) goto failed;
	}

	if((mhi->flags & MHF_HAS_R2)||(mhi->flags & MHF_HAS_FSK2)) {
		err = hdf_set_int_value(chan_nod, "PTT2", 1);
		if(err != STATUS_OK) goto failed;
	}
	if(mhi->flags & MHF_HAS_AUX) {
		err = hdf_set_int_value(chan_nod, "AUX", 1);
		if(err != STATUS_OK) goto failed;
	}
	if(mhi->flags & MHF_HAS_WINKEY) {
		err = hdf_set_int_value(chan_nod, "WK", 1);
		if(err != STATUS_OK) goto failed;
	}
	if(mhi->flags & MHF_HAS_FSK1) {
		err = hdf_set_int_value(chan_nod, "FSK1", 1);
		if(err != STATUS_OK) goto failed;
	}
	if(mhi->flags & MHF_HAS_FSK2) {
		err = hdf_set_int_value(chan_nod, "FSK2", 1);
		if(err != STATUS_OK) goto failed;
	}
	if(mhi->flags & MHF_HAS_MCP_SUPPORT) {
		err = hdf_set_int_value(chan_nod, "MCP", 1);
		if(err != STATUS_OK) goto failed;
	}
	if(mhi->flags & MHF_HAS_ROTATOR_SUPPORT) {
		err = hdf_set_int_value(chan_nod, "ROTATOR", 1);
		if(err != STATUS_OK) goto failed;
	}

	// Keyer parameters
	err = hdf_get_node(knod, "param", &param_nod);
	if(err != STATUS_OK) goto failed;
	dbg1("%s set cfg from kopts", serial);
	mhc_kopts_to_cfg(dev->ctl, (struct cfg *)param_nod);

	// Keyer channel speed

	{
		int i;
		char name[33];
		const struct cfg *speed_cfg;
		char *p;
		char buf[128];
		name[32] = 0;

		for(i = 0; i < MH_NUM_CHANNELS; i++) {
			if((speed_cfg = mhc_get_speed_cfg(dev->ctl, i))) {
				strncpy(name, ch_channel2str_new(i, mhc_get_mhinfo(dev->ctl)), sizeof(name) - 1);
				for (p = name ; *p; ++p) *p = tolower(*p);

				snprintf(buf, sizeof(buf)-1, "channel.%s", name);
				err = hdf_copy(knod, buf, (HDF *)speed_cfg);
				if(err != STATUS_OK) goto failed;
			}
		}
	}

	// CW / FSK messages
	int i;
	err = hdf_get_node(knod, "cw_messages", &cw_messages_nod);
	if(err != STATUS_OK) goto failed;

	for(i = 1; i <= 9; i++) {
		const char *text;
		HDF *idx_nod;
		uint8_t next_idx, delay;
		char idx_str[2];
		text = mhc_get_cw_message(dev->ctl, i, &next_idx, &delay);
		if(!text)
			continue;
		snprintf(idx_str, 2, "%d", i);
		err = hdf_get_node(cw_messages_nod, idx_str, &idx_nod);
		if(err != STATUS_OK) goto failed;
		hdf_set_value(idx_nod, "text", text);
		hdf_set_int_value(idx_nod, "next_idx", next_idx);
		hdf_set_int_value(idx_nod, "delay", delay);
	}

	err = hdf_get_node(knod, "fsk_messages", &fsk_messages_nod);
	if(err != STATUS_OK) goto failed;

	for(i = 1; i <= 9; i++) {
		const char *text;
		HDF *idx_nod;
		uint8_t next_idx, delay;
		char idx_str[2];
		text = mhc_get_fsk_message(dev->ctl, i, &next_idx, &delay);
		if(!text)
			continue;
		snprintf(idx_str, 2, "%d", i);
		err = hdf_get_node(fsk_messages_nod, idx_str, &idx_nod);
		if(err != STATUS_OK) goto failed;
		hdf_set_value(idx_nod, "text", text);
		hdf_set_int_value(idx_nod, "next_idx", next_idx);
		hdf_set_int_value(idx_nod, "delay", delay);
	}


	// Winkey config
	if((mhi->flags & MHF_HAS_WINKEY)) {
		if(!dev->wkman) {
			dev->wkman = wkm_create(cfgmgr->loop, dev);
		}
		err = hdf_get_node(knod, "winkey", &winkey_nod);
		if(err != STATUS_OK) goto failed;
		wkm_opts_to_cfg(dev->wkman, (struct cfg *)winkey_nod);
	}

	// SM
	struct sm *sm;
	if((sm = mhc_get_sm(dev->ctl))) {
		HDF *smnod;
		err = hdf_get_node(knod, "sm", &smnod);
		if(err != STATUS_OK) goto failed;

		sm_antsw_to_cfg(sm, (struct cfg *)smnod);
	}

#if 0
	STRING str;
	string_init(&str);
	hdf_dump_str(hdf, "", 0, &str);
	info("Updated dev, dump:\n%s", str.buf);
	string_clear(&str);
#endif
	return 0;

 failed:
	{
	STRING str;
	string_init(&str);
	nerr_error_string(err, &str);
	err("%s %s", __func__, str.buf);
	string_clear(&str);
	nerr_ignore(&err);
	return -1;
	}
}

// merge current configuration into cfg
int cfgmgr_merge_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg) {
	struct device *dev;
	int rval = 0;

	dbg1("%s()", __func__);

	rval |= cfg_set_value(cfg, "mhuxd.daemon.loglevel", log_get_level_str());
	rval |= cfg_merge(cfg, cfgmgr->runtime_cfg);

	PG_SCANLIST(dmgr_get_device_list(), dev) {
		rval |= merge_device_cfg(cfgmgr, dev, cfg);
	}

	// connector configuration from runtime cfg
	struct cfg *dest_cfg = cfg_create_child(cfg, "mhuxd.connector");
	struct cfg *src_cfg = cfg_get_child(cfgmgr->runtime_cfg, "mhuxd.run.connector");
	if(dest_cfg && src_cfg)
		//rval |= conmgr_merge_cfg(cfgmgr->conmgr, con_cfg);
		rval |= cfg_merge_s(dest_cfg, "", src_cfg);

	return rval;
}

static void completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data) {
        (void)reply_buf; (void)len;
        int *notify = user_data;
        *notify = result;
}

int cfgmgr_init(struct cfgmgr *cfgmgr) {
	HDF *saved_hdf;
	NEOERR *err;
	int rval;

	dbg1("%s()", __func__);

	err = hdf_init(&saved_hdf);
	if(err != STATUS_OK) {
		err("%s() could not initialize hdf!", __func__);
		return -1;
	}

	err = hdf_read_file(saved_hdf, CFGFILE);
	if(err != STATUS_OK) {
		log_neoerr(err, "could not read config file! (" CFGFILE ")");
		hdf_destroy(&saved_hdf);
		return 0;
	}

	rval = cfgmgr_apply_cfg(cfgmgr, (struct cfg*)saved_hdf, CFGMGR_APPLY_REPLACE);

	hdf_destroy(&saved_hdf);

	return rval;
}

static int check_icom_address(HDF *hdf) {
	NEOERR *err;
	int16_t rig, icom_address, rval = 0;

	icom_address = hdf_get_int_value(hdf, "icomaddress", 0);
	if(!icom_address) {
		rig = hdf_get_int_value(hdf, "rigtype", -1);
		if(rig > 0) {
			icom_address = rtyp_get_icom_address(rig);
			dbg0("%s() set icom address to 0x%02x", __func__, icom_address);
			err = hdf_set_int_value(hdf, "icomaddress", icom_address);
			if(err != STATUS_OK) {
				rval--;
				nerr_ignore(&err);
			}
		}
	}
	return rval;
}

static int apply_keyer_params(struct cfgmgr *cfgmgr, struct device *dev, HDF *hdf, const char *prefix) {
	HDF *phdf;
	char key[128];
	const char *val_str;
	int val, rval = 0;

	for(phdf = hdf_obj_child(hdf); phdf; phdf = hdf_obj_next(phdf)) {
		snprintf(key, sizeof(key)-1, "%s%s%s", prefix, *prefix ? "." : "", hdf_obj_name(phdf));

		if(hdf_obj_child(phdf)) {
			apply_keyer_params(cfgmgr, dev, phdf, key);
			continue;
		}

		val_str = hdf_obj_value(phdf);
		if(!val_str) {
			err("%s() no value for %s", __func__, key);
			rval++;
			continue;
		}
		val = atoi(val_str);
		//dbg1(">>> set kopt %s/%d", key, val);
		if(mhc_set_kopt(dev->ctl, key, val)) {
			err("Could not set keyer parameter %s for keyer %s!",  key, dev->serial);
			rval++;
			continue;
		}
	}

	return rval;
}

static int apply_sm_antsw_params(struct cfgmgr *cfgmgr, struct device *dev, HDF *hdf, const char *prefix) {
	HDF *phdf;
	char key[128];
	const char *val_str;
	int val, rval = 0;

	for(phdf = hdf_obj_child(hdf); phdf; phdf = hdf_obj_next(phdf)) {
		snprintf(key, sizeof(key)-1, "%s%s%s", prefix, *prefix ? "." : "", hdf_obj_name(phdf));

		if(hdf_obj_child(phdf)) {
			apply_sm_antsw_params(cfgmgr, dev, phdf, key);
			continue;
		}

		val_str = hdf_obj_value(phdf);
		if(!val_str) {
			err("%s() no value for %s", __func__, key);
			rval++;
			continue;
		}
		val = atoi(val_str);

		if(sm_antsw_set_opt(mhc_get_sm(dev->ctl), key, val)) {
			err("Could not set keyer parameter %s for keyer %s!",  key, dev->serial);
			rval++;
			continue;
		}
	}

	return rval;
}

// Apply setting from cfg to program.
//
int cfgmgr_apply_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg, int apply_mode) {
	HDF *hdf, *base_hdf = (void*)cfg;
	int rval = 0;

	dbg1("%s() in", __func__);

	for(hdf = hdf_obj_child(hdf_get_obj(base_hdf, "mhuxd.daemon")); hdf; hdf = hdf_obj_next(hdf)) {
		const char *sval;
		const char *name = hdf_obj_name(hdf);
		if(!strcmp(name, "loglevel")) {
			sval = hdf_obj_value(hdf);
			if(-1 == log_set_level_by_str(sval)) {
				err("invalid log level '%s'!", sval);
				rval++;
				continue;
			}
			continue;
		}
		warn("unkown daemon parameter '%s'!", name);
	}

	for(hdf = hdf_obj_child(hdf_get_obj(base_hdf, "mhuxd.keyer")); hdf; hdf = hdf_obj_next(hdf)) {
		const char *serial = hdf_obj_name(hdf);
		HDF *chan_hdf, *winkey_hdf;
		int result;

		struct device *dev = dmgr_get_device(serial);
		if(!dev) {
			uint16_t type = hdf_get_int_value(hdf, "type", MHT_UNKNOWN);
			dev = dmgr_add_device(serial, type);
		}

		if(!dev)
			continue;

		for(chan_hdf = hdf_obj_child(hdf_get_obj(hdf, "channel")); chan_hdf; chan_hdf = hdf_obj_next(chan_hdf)) {
			const char *chan_name = hdf_obj_name(chan_hdf);

			dbg1("%s() set speed for %s", __func__, chan_name);

			int channel = ch_str2channel(chan_name);
			if(channel < 0 || channel >= MH_NUM_CHANNELS) {
				err("can't set speed, invalid channel (%s) specified!", chan_name);
				rval++;
				continue;
			}

			check_icom_address(chan_hdf);

			result = -1;
			if(mhc_set_speed(dev->ctl, channel, (struct cfg *)chan_hdf, completion_cb, &result)) {
				err("could not set channel speed for %s!", chan_name);
				rval++;
				continue;
			}

			while(result == -1) {
				ev_loop(cfgmgr->loop, EVRUN_ONCE);
			} 

			if(result != CMD_RESULT_OK) {
				err("error setting channel speed for %s!", chan_name);
				rval++;
				continue;
			}
		}

		HDF *param_hdf = hdf_get_obj(hdf, "param");
		rval += apply_keyer_params(cfgmgr, dev, param_hdf, "");
		mk1_set_frbase(dev, (struct cfg *)param_hdf);

		if(mhc_is_online(dev->ctl)) {
			result = -1;
			if(mhc_load_kopts(dev->ctl, completion_cb, &result)) {
				err("could not write config to keyer %s!", serial);
				rval++;
			} else {
				while(result == -1) {
					ev_loop(cfgmgr->loop, EVRUN_ONCE);
				}
				if(result != CMD_RESULT_OK) {
					err("error writing config to keyer %s", serial);
					rval++;
				}
			}
		}

		// CW / FSK messages
		HDF *msg_hdf;
		for(msg_hdf = hdf_obj_child(hdf_get_obj(hdf, "cw_messages")); msg_hdf; msg_hdf = hdf_obj_next(msg_hdf)) {
			const char *idx_str = hdf_obj_name(msg_hdf);
			const char *text;
			uint8_t idx = atoi(idx_str);
			result = -1;

			text = hdf_get_value(msg_hdf, "text", NULL);
			if(text == NULL) {
				err("%s %s() index %d text missing!", serial, __func__, idx);
				continue;
			}

			if(mhc_store_cw_message(dev->ctl, idx, text, 0xff, 0, completion_cb, &result)) {
				err("%s could not store cw message on index %d", serial, idx);
				rval++;
			} else {
				while(result == -1) {
					ev_loop(cfgmgr->loop, EVRUN_ONCE);
				}
				if(result != CMD_RESULT_OK) {
					err("%s error store cw message %d", serial, idx);
					rval++;
				}
			}
		}

		for(msg_hdf = hdf_obj_child(hdf_get_obj(hdf, "fsk_messages")); msg_hdf; msg_hdf = hdf_obj_next(msg_hdf)) {
			const char *idx_str = hdf_obj_name(msg_hdf);
			const char *text;
			uint8_t idx = atoi(idx_str);
			result = -1;

			text = hdf_get_value(msg_hdf, "text", NULL);
			if(text == NULL) {
				err("%s %s() index %d text missing!", serial, __func__, idx);
				continue;
			}

			if(mhc_store_fsk_message(dev->ctl, idx, text, 0xff, 0, completion_cb, &result)) {
				err("%s could not store fsk message on index %d", serial, idx);
				rval++;
			} else {
				while(result == -1) {
					ev_loop(cfgmgr->loop, EVRUN_ONCE);
				}
				if(result != CMD_RESULT_OK) {
					err("%s error store fsk message %d", serial, idx);
					rval++;
				}
			}
		}

		// Winkey
		const struct mh_info *mhi;
		mhi = mhc_get_mhinfo(dev->ctl);

		if((mhi->flags & MHF_HAS_WINKEY) && !dev->wkman)
			dev->wkman = wkm_create(cfgmgr->loop, dev);
		
		if(dev->wkman) {
			int werr,val;
			for(winkey_hdf = hdf_obj_child(hdf_get_obj(hdf, "winkey")); winkey_hdf; winkey_hdf = hdf_obj_next(winkey_hdf)) {
				const char *key = hdf_obj_name(winkey_hdf);
				const char *val_str = hdf_obj_value(winkey_hdf);
				if(!val_str || !*val_str)
					continue;
				val = atoi(val_str);
				if(wkm_set_value(dev->wkman, key, val))
					rval++;
			}

			if(mhc_is_online(dev->ctl)) {
				werr = wkm_write_cfg(dev->wkman);
				if(WKM_RESULT_OK != werr) {
					err("could not write config to winkey (%s)!", wkm_err_string(werr));
					rval++;
				}
			}
		}

		// SM
		struct sm *sm;
		HDF *smhdf;
		if((sm = mhc_get_sm(dev->ctl)) && (smhdf = hdf_get_obj(hdf, "sm"))) {
			// fixed area
			HDF *phdf = hdf_get_obj(smhdf, "fixed");
			if(phdf)
				rval += apply_sm_antsw_params(cfgmgr, dev, phdf, "");

			// output
			for(phdf = hdf_obj_child(hdf_get_obj(hdf, "sm.output")); phdf; phdf = hdf_obj_next(phdf)) {
				const char *key = hdf_obj_name(phdf);
				const char *val_str = hdf_obj_value(phdf);
				if(!val_str || !*val_str)
					continue;
				int val = atoi(val_str);
				if(sm_antsw_set_output(sm, key, val))
					rval++;

			}
			
			if(apply_mode == CFGMGR_APPLY_REPLACE)
				sm_antsw_clear_lists(sm);

			// objects
			for(phdf = hdf_obj_child(hdf_get_obj(hdf, "sm.obj")); phdf; phdf = hdf_obj_next(phdf)) {
				if(sm_antsw_add_obj(sm, (struct cfg *)phdf))
					rval++;
			}
		}
	}

	struct cfg *pcfg;
	for(pcfg = cfg_first_child( cfg_get_child(cfg, "mhuxd.connector")); pcfg; pcfg = cfg_next_child(pcfg)) {
		const char *id_str = cfg_name(pcfg);
		if(!id_str) {
			err("%s %d internal error", __func__, __LINE__);
			continue;
		}
		int id, requested_id = atoi(id_str);
		id = conmgr_create_con(cfgmgr->conmgr, cfgmgr->loop, pcfg, requested_id);
		if(!id) {
			err("failed to create connector!");
			rval++;
		}

		if(requested_id || id) {
			struct cfg *con_node;
			char buf[128];
			snprintf(buf, sizeof(buf), "mhuxd.run.connector.%d", id ? id : requested_id);
			con_node = cfg_create_child(cfgmgr->runtime_cfg, buf);
			if(con_node) {
				cfg_merge(con_node, pcfg);
				cfg_set_value(con_node, "status", id ? "ok" : "failed");

			} else {
				rval++;
			}

		}

#if 0
		if(old_id || id) {
			// So this connector has either been successfully created.
			// Or is has been previously created successfully (old_id != 0). 
			// We want to store it in any of both cases in live_hdf.

			char buf[128];
			int real_id = id ? id : old_id;
			snprintf(buf, sizeof(buf)-1, "mhuxd.connector.%d", real_id);
			err = hdf_copy(cfgmgr->hdf_live, buf, hdf);
			nerr_ignore(&err);

			snprintf(buf, sizeof(buf)-1, "mhuxd.run.connector.%d.status", real_id);
			err = hdf_set_value(cfgmgr->hdf_live, buf, id ? "ok" : "failed");
			nerr_ignore(&err);
		}
#endif
	}


#if 0
	STRING str;
	string_init(&str);
	hdf_dump_str(cfgmgr->hdf_live, "", 0, &str);
	//hdf_dump_str(qhdf, "", 0, &str);
	info("%s dump:", __func__);
	info("%s", str.buf);
	string_clear(&str);
#endif

	dbg1("%s() out", __func__);

	return rval;
}

int cfgmgr_remove(struct cfgmgr *cfgmgr, struct cfg *cfg) {
	int rval = 0;
	struct cfg *pcfg;

	dbg1("%s()", __func__);

	// connectors
	for(pcfg = cfg_first_child( cfg_get_child(cfg, "mhuxd.connector")); pcfg; pcfg = cfg_next_child(pcfg)) {
		int id = cfg_name_to_int(pcfg, -1);
		if(id == -1) {
			rval++;
			continue;
		}
		conmgr_destroy_con(cfgmgr->conmgr, id);
		cfg_remove_child_i(cfgmgr->runtime_cfg, "mhuxd.run.connector", id);
	}

	// SM
	for(pcfg = cfg_first_child( cfg_get_child(cfg, "mhuxd.keyer")); pcfg; pcfg = cfg_next_child(pcfg)) {
		const char *serial = cfg_name(pcfg);
		struct device *dev = dmgr_get_device(serial);
		struct cfg *smcfg;
		if(!dev) {
			err("%s() could not find device %s in device list!", __func__, serial);
			continue;
		}

		for(smcfg = cfg_first_child( cfg_get_child(pcfg, "sm.obj")); smcfg; smcfg = cfg_next_child(smcfg)) {
			int id = cfg_name_to_int(smcfg, -1);
			if(id == -1) {
				rval++;
				continue;
			}

			struct cfg *tmp_cfg = cfg_get_child(smcfg, "ref");
			if(tmp_cfg) {
				// remove references only
				struct cfg *ref_cfg;
				for(ref_cfg = cfg_first_child(tmp_cfg); ref_cfg; ref_cfg = cfg_next_child(ref_cfg)) {

					if(sm_antsw_rem_obj_ref(mhc_get_sm(dev->ctl), id, cfg_name_to_int(ref_cfg, -1)))
						rval++;
				}

			} else {
				// remove the whole thing
				if(sm_antsw_rem_obj(mhc_get_sm(dev->ctl), id))
					rval++;
			}
		}
	}

	return rval;
}

int cfgmgr_modify(struct cfgmgr *cfgmgr, struct cfg *cfg) {
	(void)cfgmgr;
	int rval = 0;
	struct cfg *pcfg;

	dbg1("%s()", __func__);

	// connectors, modify not supported
	
	// SM
	for(pcfg = cfg_first_child( cfg_get_child(cfg, "mhuxd.keyer")); pcfg; pcfg = cfg_next_child(pcfg)) {
		const char *serial = cfg_name(pcfg);
		struct device *dev = dmgr_get_device(serial);
		struct cfg *smcfg;
		if(!dev) {
			err("%s() could not find device %s in device list!", __func__, serial);
			continue;
		}

		for(smcfg = cfg_first_child( cfg_get_child(pcfg, "sm.obj")); smcfg; smcfg = cfg_next_child(smcfg)) {
			int id = cfg_name_to_int(smcfg, -1);
			if(id == -1) {
				rval++;
				continue;
			}

			if(sm_antsw_mod_obj(mhc_get_sm(dev->ctl), smcfg))
				rval++;
		}
	}

	return rval;
}

int cfgmgr_sm_load(const char *serial) {
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

int cfgmgr_sm_store(const char *serial) {
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


struct cfgmgr *cfgmgr_create(struct conmgr *conmgr, struct ev_loop *loop) {
	dbg1("%s()", __func__);
	struct cfg *runtime_cfg = cfg_create();
	if(!runtime_cfg)
		return NULL;

	struct cfgmgr *cfgmgr = w_calloc(1, sizeof(*cfgmgr));
	cfgmgr->loop = loop;
	cfgmgr->conmgr = conmgr;
	cfgmgr->runtime_cfg = runtime_cfg;
	merge_runtime_cfg(cfgmgr->runtime_cfg);
	return cfgmgr;
}

int cfgmgr_save_cfg(struct cfgmgr *cfgmgr) {
	NEOERR *err;
	HDF *save_hdf;
	int rval = 0;

	dbg1("%s()", __func__);

	err = hdf_init(&save_hdf);
	if(err != STATUS_OK)  {
		nerr_ignore(&err);
		return -1;
	}

	cfgmgr_merge_cfg(cfgmgr, (void*)save_hdf);

	//err = hdf_remove_tree(save_hdf, "mhuxd.run");

	err = hdf_write_file_atomic(save_hdf, CFGFILE);
	if(err != STATUS_OK) {
		STRING str;
		string_init(&str);
		nerr_error_string(err, &str);
		err("could not save configuration! (%s)", str.buf);
		nerr_ignore(&err);
		string_clear(&str);
		rval = -1;
	}

	hdf_destroy(&save_hdf);
	return rval;
}

void cfgmgr_destroy(struct cfgmgr *cfgmgr) {
	dbg1("%s()", __func__);

	if(!cfgmgr)
		return;

	if(cfgmgr->runtime_cfg)
		cfg_destroy(cfgmgr->runtime_cfg);

	free(cfgmgr);
}



