/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <unistd.h>
#include <sys/types.h>
#include <util/neo_hdf.h>
#include <ev.h>
#include "cfgmgr.h"
#include "logger.h"
#include "util.h"
#include "version.h"
#include "devmgr.h"
#include "mhcontrol.h"
#include "mhinfo.h"
#include "conmgr.h"
#include "radiotypes.h"
#include "channel.h"
#include "wkman.h"
#include "mhsm.h"

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
	HDF *hdf_saved;
	HDF *hdf_live;
};

static void log_neoerr(NEOERR *err, const char *what) {
	STRING str;

	string_init(&str);

	nerr_error_string(err, &str);
	err("(cfgmgr) %s(%s)", what, str.buf ? str.buf : "Unkown");
	string_clear(&str);
	nerr_ignore(&err);
}

static void initialize_live_hdf(HDF *hdf) {
	NEOERR *err;
	char buf[HOST_NAME_MAX + 1];
	gethostname(buf, HOST_NAME_MAX);
	err = hdf_set_value(hdf, "mhuxd.run.program.name", _package);
	if(err) nerr_ignore(&err);

	err = hdf_set_value(hdf, "mhuxd.run.program.version", _package_version);
	if(err) nerr_ignore(&err);

	err = hdf_set_value(hdf, "mhuxd.run.hostname", buf);
	if(err) nerr_ignore(&err);

	err = hdf_set_value(hdf, "mhuxd.run.logfile", log_file_name);
	if(err) nerr_ignore(&err);

	err = hdf_set_int_value(hdf, "mhuxd.run.pid", getpid());
	if(err) nerr_ignore(&err);

	err = hdf_set_value(hdf, "mhuxd.daemon.loglevel", log_get_level_str()); 
	if(err) nerr_ignore(&err);

	int i;
	for(i = 0; i < num_rig_types; i++) {
		char buf[128];
		snprintf(buf, sizeof(buf)-1, "mhuxd.rigtype.%d.name", rig_types[i].key);
		err = hdf_set_value(hdf, buf, rig_types[i].name);
		if(err) nerr_ignore(&err);

		snprintf(buf, sizeof(buf)-1, "mhuxd.rigtype.%d.icom_addr", rig_types[i].key);
		err = hdf_set_int_value(hdf, buf, rig_types[i].icom_addr);
		if(err) nerr_ignore(&err);
	}
}

#if 0
int update_hdf_kopt(const char *serial, const char *key, int val) {
	struct cfgmgr *cfgmgr = dmgr_get_cfgmgr();
	NEOERR *err;
	char full_key[MAX_HDF_PATH_LEN + 1];

	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.keyer.%s.param.%s", serial, key);
	err = hdf_set_int_value(cfgmgr->hdf_live, full_key, val);
	if(err != STATUS_OK) {
		log_neoerr(err, __func__);
		return -1;
	}

	return 0;
}
#endif

enum {
        MOD_CW,
        MOD_VOICE,
	MOD_FSK,
        MOD_DIGITAL,
};

static int mk1_care_frbase(struct cfgmgr *cfgmgr, struct device *dev) {
	int rval = 0;

	// Ugly special treatment for MK1
	const struct mh_info *mhi;
	mhi = mhc_get_mhinfo(dev->ctl);
	if(mhi && mhi->type == MHT_MK ) {
		HDF *khdf;
		char buf[128];
		int mode, audioRx = -1, audioTx = -1, audioTxFootSw = -1, ptt1 = -1, ptt2 = -1;

		snprintf(buf, sizeof(buf), "mhuxd.keyer.%s.param", dev->serial);
		khdf = hdf_get_obj(cfgmgr->hdf_live, buf);
		if(!khdf) {
			rval++;
			err("(cfgmgr) %s() keyer hdf not found for %s", __func__, dev->serial);
			goto out;
		}
	
		mode = hdf_get_int_value(khdf, "r1KeyerMode", -1);
		switch(mode) {
		case MOD_CW:
			audioRx = hdf_get_int_value(khdf, "r1FrBase_Cw.audioRx", -1);
			audioTx = hdf_get_int_value(khdf, "r1FrBase_Cw.audioTx", -1);
			audioTxFootSw = hdf_get_int_value(khdf, "r1FrBase_Cw.audioTxFootSw", -1);
			ptt1 = hdf_get_int_value(khdf, "r1FrBase_Cw.ptt1", -1);
			ptt2 = hdf_get_int_value(khdf, "r1FrBase_Cw.ptt2", -1);
			break;
		case MOD_VOICE:
			audioRx = hdf_get_int_value(khdf, "r1FrBase_Voice.audioRx", -1);
			audioTx = hdf_get_int_value(khdf, "r1FrBase_Voice.audioTx", -1);
			audioTxFootSw = hdf_get_int_value(khdf, "r1FrBase_Voice.audioTxFootSw", -1);
			ptt1 = hdf_get_int_value(khdf, "r1FrBase_Voice.ptt1", -1);
			ptt2 = hdf_get_int_value(khdf, "r1FrBase_Voice.ptt2", -1);
			break;
		case MOD_FSK:
		case MOD_DIGITAL:
			audioRx = hdf_get_int_value(khdf, "r1FrBase_Digital.audioRx", -1);
			audioTx = hdf_get_int_value(khdf, "r1FrBase_Digital.audioTx", -1);
			audioTxFootSw = hdf_get_int_value(khdf, "r1FrBase_Digital.audioTxFootSw", -1);
			ptt1 = hdf_get_int_value(khdf, "r1FrBase_Digital.ptt1", -1);
			ptt2 = hdf_get_int_value(khdf, "r1FrBase_Digital.ptt2", -1);
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

 out:
	return rval;
}

static int apply_keyer_params(struct cfgmgr *cfgmgr, struct device *dev, HDF *hdf, const char *prefix) {
	NEOERR *err;
	HDF *phdf;
	char key[128], full_key[128];
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
			err("cfgmgr) %s() no value for %s", __func__, key);
			rval++;
			continue;
		}
		val = atoi(val_str);
		//dbg1(">>> set kopt %s/%d", key, val);
		if(mhc_set_kopt(dev->ctl, key, val)) {
			err("(cfgmgr) Could not set keyer parameter %s for keyer %s!",  key, dev->serial);
			rval++;
			continue;
		}

		snprintf(full_key, sizeof(full_key)-1, "mhuxd.keyer.%s.param.%s", dev->serial, key);
		err = hdf_set_int_value(cfgmgr->hdf_live, full_key, val);
		if(err != STATUS_OK) {
			err("(cfgmgr) %s() could not update parameter %s in live hdf!", __func__, full_key);
			nerr_ignore(&err);
			rval++;
		}
	}

	return rval;
}

void cfgmr_state_changed_cb(const char *serial, int state, void *user_data) {
	(void)state; 
	struct cfgmgr *cfgmgr = user_data;
	dbg1("(cfgmgr) %s() %s", __func__, mhc_state_str(state));
	cfgmgr_update_hdf_dev(cfgmgr, serial);
}

int cfgmgr_update_hdf_all(struct cfgmgr *cfgmgr) {
	struct device *dev;
	int rval = 0;
	PG_SCANLIST(dmgr_get_device_list(), dev)
		rval += cfgmgr_update_hdf_dev(cfgmgr, dev->serial);
	return rval;
}

int cfgmgr_update_hdf_dev(struct cfgmgr *cfgmgr, const char *serial) {
	NEOERR *err;
	char full_key[MAX_HDF_PATH_LEN + 1];
	const struct mh_info *mhi;

	dbg1("(cfgmgr) %s()", __func__);

	struct device *dev = dmgr_get_device(serial);
	if(!dev) {
		err("(cfgmgr) %s() could not find device %s in device list!", __func__, serial);
		return -1;
	}
	mhi = mhc_get_mhinfo(dev->ctl);

	// Keyer node
	HDF *knod, *param_nod, *flags_nod, *chan_nod, *run_nod, *winkey_nod;

	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.keyer.%s", serial);
	err = hdf_get_node(cfgmgr->hdf_live, full_key, &knod);
	if(err != STATUS_OK) goto failed;

	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.run.keyer.%s", serial);
	err = hdf_get_node(cfgmgr->hdf_live, full_key, &run_nod);
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
	err = hdf_get_node(cfgmgr->hdf_live, full_key, &flags_nod);
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


	// Keyer channels

	snprintf(full_key, MAX_HDF_PATH_LEN, "mhuxd.run.keyer.%s.channels", serial);
	err = hdf_get_node(cfgmgr->hdf_live, full_key, &chan_nod);
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
	/*
	if(hdf_obj_child(param_nod)) {
		dbg1("(cfgmgr) set kopts from cfg");
		apply_keyer_params(cfgmgr, dev, param_nod, "");
	} else {
	*/
	dbg1("(cfgmgr) %s set cfg from kopts", serial);
	mhc_kopts_to_cfg(dev->ctl, (struct cfg *)param_nod);
		/*
	}
		*/

	// Winkey config
	if((mhi->flags & MHF_HAS_WINKEY)) {
		if(!dev->wkman) {
			dev->wkman = wkm_create(cfgmgr->loop, dev);
		}
		
#if 1
		err = hdf_get_node(knod, "winkey", &winkey_nod);
		if(err != STATUS_OK) goto failed;

		//		if(!hdf_obj_child(winkey_nod))
		wkm_opts_to_cfg(dev->wkman, (struct cfg *)winkey_nod);
			//		else
			//			wkm_cfg_to_opts(dev->wkman, (struct cfg *)winkey_nod);
#endif
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
	hdf_dump_str(cfgmgr->hdf_live, "", 0, &str);
	info("Updated dev, dump:\n%s", str.buf);
	string_clear(&str);
#endif
	return 0;

 failed:
	{
	STRING str;
	string_init(&str);
	nerr_error_string(err, &str);
	err("(cfgmgr) %s %s", __func__, str.buf);
	string_clear(&str);
	nerr_ignore(&err);
	return -1;
	}
}

struct cfg *cfgmgr_get_live_cfg(struct cfgmgr *cfgmgr) {
	return (void*)cfgmgr->hdf_live;
}

static void completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data) {
        (void)reply_buf; (void)len;
        int *notify = user_data;
        *notify = result;
}
#if 0
static int param_update(struct device *dev, HDF *hdf, const char *key, const char *subkey, const char *val_str) {
	size_t klen = strlen(key);
	int rval = 0;

	char *str = w_malloc(klen + strlen(subkey) + 2);
	strcpy(str, key);
	if(subkey && *subkey) {
		str[klen] = '.';
		strcpy(str + klen + 1, subkey);
	}

	if(!val_str) {
		err("cfgmgr) %s() no value for %s", __func__, str);
		rval++;
		goto out;
	}
	int val = atoi(val_str);
	if(mhc_set_kopt(dev->ctl, str, val)) {
		err("(cfgmgr) Could not set keyer parameter %s for keyer %s!",  str, dev->serial);
		rval++;
		goto out;
	}
	char buf[128];
	NEOERR *err;
	snprintf(buf, sizeof(buf)-1, "mhuxd.keyer.%s.param.%s", dev->serial, str);
	err = hdf_set_int_value(hdf, buf, val);
	if(err != STATUS_OK) {
		err("(cfgmgr) %s() could not update parameter %s in live hdf!", __func__, key);
		nerr_ignore(&err);
		rval++;
	}

out:
	free(str);

	return rval;
}
#endif

int cfgmgr_init(struct cfgmgr *cfgmgr) {
	HDF *saved_hdf;
	NEOERR *err;
	int rval;

	dbg1("(cfgmgr) %s()", __func__);

	err = hdf_init(&saved_hdf);
	if(err != STATUS_OK) {
		err("(cfgmgr) %s() could not initialize hdf!", __func__);
		return -1;
	}

	err = hdf_read_file(saved_hdf, CFGFILE);
	if(err != STATUS_OK) {
		log_neoerr(err, "could not read config file! (" CFGFILE ")");
		hdf_destroy(&saved_hdf);
		return 0;
	}

	rval = cfgmgr_apply_cfg(cfgmgr, (struct cfg*)saved_hdf);

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
			dbg0("(cfgmgr) %s() set icom address to 0x%02x", __func__, icom_address);
			err = hdf_set_int_value(hdf, "icomaddress", icom_address);
			if(err != STATUS_OK) {
				rval--;
				nerr_ignore(&err);
			}
		}
	}
	return rval;
}

int cfgmgr_apply_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg) {
	NEOERR *err;
	HDF *hdf, *base_hdf = (void*)cfg;
	int rval = 0;

	dbg1("(cfgmgr) %s() in", __func__);

	for(hdf = hdf_obj_child(hdf_get_obj(base_hdf, "mhuxd.daemon")); hdf; hdf = hdf_obj_next(hdf)) {
		const char *sval;
		const char *name = hdf_obj_name(hdf);
		if(!strcmp(name, "loglevel")) {
			sval = hdf_obj_value(hdf);
			if(-1 != log_set_level_by_str(sval)) {
				err = hdf_set_value(cfgmgr->hdf_live, "mhuxd.daemon.loglevel", sval ? sval : "NULL");
				nerr_ignore(&err);
				continue;
			}
			err("(cfgmgr) invalid log level '%s'!", sval);
			rval++;
			continue;
		}
		warn("(cfgmgr) unkown daemon parameter '%s'!", name);
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

			dbg1("(cfgmgr) %s() set speed for %s", __func__, chan_name);

			int channel = ch_str2channel(chan_name);
			if(channel < 0 || channel >= MH_NUM_CHANNELS) {
				err("(ctl) can't set speed, invalid channel (%s) specified!", chan_name);
				rval++;
				continue;
			}

			check_icom_address(chan_hdf);

			result = -1;
			if(mhc_set_speed(dev->ctl, channel, (struct cfg *)chan_hdf, completion_cb, &result)) {
				err("(cfgmgr) could not set channel speed for %s!", chan_name);
				rval++;
				continue;
			}

			while(result == -1) {
				ev_loop(cfgmgr->loop, EVRUN_ONCE);
			} 

			if(result != CMD_RESULT_OK) {
				err("(cfgmgr) error setting channel speed for %s!", chan_name);
				rval++;
				continue;
			}

			char buf[128];
			snprintf(buf, sizeof(buf)-1, "mhuxd.keyer.%s.channel.%s", serial, chan_name);
			err = hdf_remove_tree(cfgmgr->hdf_live, buf);
			nerr_ignore(&err);

			err = hdf_copy(cfgmgr->hdf_live, buf, chan_hdf);
			nerr_ignore(&err);
		}

		rval += apply_keyer_params(cfgmgr, dev, hdf_get_obj(hdf, "param"), "");
		mk1_care_frbase(cfgmgr, dev);

		if(mhc_is_online(dev->ctl)) {
			result = -1;
			if(mhc_load_kopts(dev->ctl, completion_cb, &result)) {
				err("(cfgmgr) could not write config to keyer %s!", serial);
				rval++;
			} else {
				while(result == -1) {
					ev_loop(cfgmgr->loop, EVRUN_ONCE);
				}
				if(result != CMD_RESULT_OK) {
					err("(cfgmgr) error writing config to keyer %s", serial);
					rval++;
				}
			}
		}

		if(dev->wkman) {
			int werr,val;
			char buf[128];
			NEOERR *err;
			HDF *winkey_nod;
			for(winkey_hdf = hdf_obj_child(hdf_get_obj(hdf, "winkey")); winkey_hdf; winkey_hdf = hdf_obj_next(winkey_hdf)) {
				const char *key = hdf_obj_name(winkey_hdf);
				const char *val_str = hdf_obj_value(winkey_hdf);
				if(!val_str || !*val_str)
					continue;
				val = atoi(val_str);
				if(wkm_set_value(dev->wkman, key, val))
					rval++;
			}

			snprintf(buf, sizeof(buf)-1, "mhuxd.keyer.%s.winkey", dev->serial);
			err = hdf_get_node(cfgmgr->hdf_live, buf, &winkey_nod);
			if(err == STATUS_OK) {
				wkm_opts_to_cfg(dev->wkman, (struct cfg *)winkey_nod);
			} else
				nerr_ignore(&err);

			if(mhc_is_online(dev->ctl)) {
				werr = wkm_write_cfg(dev->wkman);
				if(WKM_RESULT_OK != werr) {
					err("(cfgmgr) could not write config to winkey (%s)!", wkm_err_string(werr));
					rval++;
				}
			}
		}
	}

	for(hdf = hdf_obj_child(hdf_get_obj(base_hdf, "mhuxd.connector")); hdf; hdf = hdf_obj_next(hdf)) {
		const char *id_str = hdf_obj_name(hdf);
		if(!id_str) {
			err("(cfgmgr) %s %d internal error", __func__, __LINE__);
			continue;
		}
		int id, old_id = atoi(id_str);
		id = conmgr_create_con(cfgmgr->conmgr, cfgmgr->loop, (struct cfg *)hdf, old_id);
		if(!id) {
			err("(cfgmgr) failed to create connector!");
			rval++;
		}

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

	dbg1("(cfgmgr) %s() out", __func__);

	return rval;
}

int cfgmgr_unset_cfg(struct cfgmgr *cfgmgr, struct cfg *cfg) {
	NEOERR *err;
	HDF *hdf, *base_hdf = (void*)cfg;
	int rval = 0;

	for(hdf = hdf_obj_child(hdf_get_obj(base_hdf, "mhuxd.connector")); hdf; hdf = hdf_obj_next(hdf)) {
		const char *id_str = hdf_obj_name(hdf);
		if(!id_str) {
			rval++;
			continue;
		}
		int id = atoi(id_str);
		if(conmgr_destroy_con(cfgmgr->conmgr, id)) {
			err("(cfgmgr) could not destroy connector %d, not found!", id);
			rval++;
			continue;
		}
		char buf[128];
		snprintf(buf, sizeof(buf)-1, "mhuxd.connector.%d", id);
		err = hdf_remove_tree(cfgmgr->hdf_live, buf);
		if(err != STATUS_OK)
			err("(cfgmgr) %s() error removing '%s' from live tree", __func__, buf);
		nerr_ignore(&err);
	}

	return rval;
}

struct cfgmgr *cfgmgr_create(struct conmgr *conmgr, struct ev_loop *loop) {
	NEOERR *err = STATUS_OK;

	dbg1("(cfgmgr) %s()", __func__);

	struct cfgmgr *cfgmgr = w_calloc(1, sizeof(*cfgmgr));
	cfgmgr->loop = loop;
	cfgmgr->conmgr = conmgr;

	if(err == STATUS_OK)
		err = hdf_init(&cfgmgr->hdf_live);

	if(err != STATUS_OK) {
		log_neoerr(err, "could not initialize HDF!");
		goto failed;
	}

	initialize_live_hdf(cfgmgr->hdf_live);

	return cfgmgr;

 failed:
	hdf_destroy(&cfgmgr->hdf_live);
	free(cfgmgr);
	return NULL;
}

int cfgmgr_save_cfg(struct cfgmgr *cfgmgr) {
	NEOERR *err;
	HDF *save_hdf, *p_hdf;

	dbg1("(cfgmgr) %s()", __func__);

	err = hdf_init(&save_hdf);
	if(err != STATUS_OK) goto fail;

	p_hdf = hdf_get_obj(cfgmgr->hdf_live, "mhuxd.daemon");
	if(p_hdf) {
		err = hdf_copy(save_hdf, "mhuxd.daemon", p_hdf);
		if(err != STATUS_OK) goto fail;
	}

	p_hdf = hdf_get_obj(cfgmgr->hdf_live, "mhuxd.keyer");
	if(p_hdf) {
		err = hdf_copy(save_hdf, "mhuxd.keyer", p_hdf);
		if(err != STATUS_OK) goto fail;
	}

	p_hdf = hdf_get_obj(cfgmgr->hdf_live, "mhuxd.connector");
	if(p_hdf) {
		err = hdf_copy(save_hdf, "mhuxd.connector", p_hdf);
		if(err != STATUS_OK) goto fail;
	}

	err = hdf_write_file_atomic(save_hdf, CFGFILE);
	if(err != STATUS_OK) goto fail;

	hdf_destroy(&save_hdf);
	return 0;

 fail:
	{
	STRING str;
	string_init(&str);
	nerr_error_string(err, &str);
	err("(cfgmgr) could not save configuration! (%s)", str.buf);
	nerr_ignore(&err);
	string_clear(&str);
	hdf_destroy(&save_hdf);
	}
	return -1;
}

void cfgmgr_destroy(struct cfgmgr *cfgmgr) {
	dbg1("(cfgmgr) %s()", __func__);

	if(!cfgmgr)
		return;

	hdf_destroy(&cfgmgr->hdf_saved);
	hdf_destroy(&cfgmgr->hdf_live);
	free(cfgmgr);
}



