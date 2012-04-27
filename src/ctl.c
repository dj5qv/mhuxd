/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

#include "global.h"
#include "util.h"
#include "mhproto.h"
#include "pglist.h"
#include "opts.h"
#include "ctl.h"
#include "dispatcher.h"
#include "logger.h"
#include "buffer.h"
#include "mhinfo.h"
#include "vsp.h"
#include "cfgfile.h"
#include "kcfg.h"

static int apply_cfg(struct ctl *ctl);

struct ctl {
	struct dispatcher *dp;
	struct config *cfg;
	//struct config *new_cfg;
	struct cmd_request req;
	struct cmd_request req_heartbeat;
	struct mh_info     mhi;
	struct timer	  *tmr_heartbeat;
	int    req_err;
};

static void parse_version(struct mh_info *mhi, struct cmd_request *req) {
	struct buffer *b = &req->b_resp;
	const unsigned char *p = b->data;
	if(b->size < 19)
		return;

	p++;

	int t = (p[2] << 8) | p[5];
        switch(t) {
        case 0x1000:
		switch(p[17]) {
		case 0x0f:
			mhi->type = MHT_DK;
			break;
		case 0xf0:
			mhi->type = MHT_CK;
			break;
		default:
			mhi->type = MHT_MK;
			break;
		}
		break;
        case 0x1201:
		mhi->type = MHT_MK2;
		break;
        case 0x1601:
		mhi->type = MHT_DK2;
		break;
        case 0x1101:
		mhi->type = MHT_MK2R;
		break;
        case 0x1102:
		mhi->type = MHT_MK2Rp;
		break;
        case 0x1501:
		mhi->type = MHT_U2R;
		break;
        case 0x1301:
		mhi->type = MHT_SM;
		break;
        case 0x1401:
		mhi->type = MHT_SMD;
		break;
        default:
		mhi->type = MHT_UNKNOWN;
		break;
        }

	mhi->ver_winkey = p[16];
	mhi->ver_fw_major = p[9];
	mhi->ver_fw_minor = p[10];

	int i;
	mhi->type_str = "Unkown";
        for(i = 0; i < mh_info_map_size; i++) {
		if(mhi->type == mh_info_map[i].type) {
			mhi->type_str = mh_info_map[i].name;
			mhi->flags = mh_info_map[i].flags;
			break;
		}
        }
}

static int start_vsp_ptt(struct ctl *ctl, struct vsp_config *vcfg, int radio) {
	int soctl[2] = { -1, -1 };
	int err;

	dbg0("CTL Starting VSP_PTT %s", vcfg->devname);

	err = socketpair(AF_UNIX, SOCK_STREAM, 0, soctl);
	if(err) {
		err_e(errno, "CTL Could not create ctl socket for VSP!");
		goto fail;
	}

	err = vsp_create(vcfg, -1, soctl[1]);
	if(err) {
		err("CTL Could not create VSP_PTT %s!", vcfg->devname);
		goto fail;
	}

	dp_set_ptt_fd(ctl->dp, radio, soctl[0]);

	info("CTL VSP_PTT %s started", vcfg->devname);
	return 0;

 fail:
	close(soctl[0]);
	close(soctl[1]);
	return -1;
}


static int start_vsp(struct ctl *ctl, struct vsp_config *vcfg, int ch) {
	int sodat[2] = { -1, -1 };
	int soctl[2] = { -1, -1 };
	int err;

	dbg0("CTL Starting VSP %s", vcfg->devname);

	err = socketpair(AF_UNIX, SOCK_STREAM, 0, sodat);
	if(err) {
		err_e(errno, "CTL Could not create data socket for VSP!");
		return -1;
	}

	err = socketpair(AF_UNIX, SOCK_STREAM, 0, soctl);
	if(err) {
		err_e(errno, "CTL Could not create ctl socket for VSP!");
		goto fail;
	}

        if (fcntl(sodat[0], F_SETFL, O_NONBLOCK) < 0) {
		err_e(errno, "CTL Failed to set NONBLOCK on data socket");
		goto fail;
        }

	if (fcntl(sodat[1], F_SETFL, O_NONBLOCK) < 0) {
		err_e(errno, "CTL Failed to set NONBLOCK on data socket");
		goto fail;
	}

	err = vsp_create(vcfg, sodat[1], soctl[1]);
	if(err) {
		err("CTL Could not create VSP %s!", vcfg->devname);
		goto fail;
	}

	dp_set_fd(ctl->dp, ch, sodat[0]);
	dp_set_ctl_fd(ctl->dp, ch, soctl[0]);

	info("CTL VSP %s started", vcfg->devname);
	return 0;

 fail:
	close(sodat[0]);
	close(sodat[1]);
	close(soctl[0]);
	close(soctl[1]);
	return -1;
}

static void req_done(struct cmd_request *req) {
	if(!req)
		return;

	struct ctl *ctl = req->data;

	switch(req->state) {
	case CRS_SUCCESS:
		ctl->req_err = 0;
		break;
	case CRS_TIMEOUT:
		ctl->req_err++;
		err("CMD request timed out! (%d)", ctl->req_err);
		break;
	case CRS_ERROR:
		ctl->req_err++;
		err("CMD request error! (%d)", ctl->req_err);
		break;
	}

	if(ctl->req_err >= 10) {
		err("CTL Device does not respond!");
		//dp_cancel_timer(ctl->dp, ctl->tmr_heartbeat);
		dp_terminate(ctl->dp);
	}
}

static void req_done_dealloc(struct cmd_request *req) {
	req_done(req);
	free(req);
}

static void heart_beat(struct timer *t, void *userdata) {
	struct ctl *ctl = userdata;

	if(ctl->req_heartbeat.state & CRSF_PENDING) {
		warn("CTL Can't send heartbeat, request still pending!");
		return;
	}

	buf_reset(&ctl->req_heartbeat.b_cmd);
	buf_append_c(&ctl->req_heartbeat.b_cmd, CMD_ARE_YOU_THERE);
	buf_append_c(&ctl->req_heartbeat.b_cmd, CMD_ARE_YOU_THERE | MH_BIT_MSB);
	dp_submit_request(ctl->dp, &ctl->req_heartbeat);
}

struct ctl *ctl_create(struct dispatcher *dp, struct config *cfg) {
	struct ctl *ctl = w_calloc(1, sizeof(*ctl));

	ctl->dp = dp;
	ctl->cfg = cfg;

	memset(&ctl->req, 0, sizeof(ctl->req));
	ctl->req.req_done = &req_done;
	ctl->req.data = ctl;

	ctl->req_heartbeat.req_done = &req_done;
	ctl->req_heartbeat.data = ctl;

	ctl->tmr_heartbeat = dp_create_timer(
				dp,
				cfg_get_int(ctl->cfg, "//Daemon/KeepAliveInterval", 1000),
				0, &heart_beat, ctl);

	return ctl;
}

int ctl_probe(struct ctl *ctl) {
	int i, success = 0;
	int err;

	/* Check if the device responds to ARE YOU THERE commands. */
	for(i = 0; success < 3 && i < 10; i++) {
		buf_reset(&ctl->req.b_cmd);
		buf_append_c(&ctl->req.b_cmd, CMD_ARE_YOU_THERE);
		buf_append_c(&ctl->req.b_cmd, CMD_ARE_YOU_THERE | MH_BIT_MSB);
		err = dp_submit_request(ctl->dp, &ctl->req);
		if(err)
			return -1;
		dp_loop(ctl->dp, 1);

		switch(ctl->req.state) {
		case CRS_SUCCESS:
			success++;
			break;
		case CRS_TIMEOUT:
			success = 0;
			break;
		case CRS_ERROR:
			return -1;
			break;
		}
	}

	if(success < 3) {
		return -1;
	}

	return 0;
}

int ctl_init(struct ctl *ctl) {
	int i, success = 0;
	int err;
#if 0
	/* Test */
	dp_set_keyer_fd(ctl->dp, 0);
	apply_cfg(ctl);
	dp_loop(ctl->dp,0);
	return -1;
#endif

	/* Check if the device responds to ARE YOU THERE commands. */
	for(i = 0; success < 3 && i < 10; i++) {
		buf_reset(&ctl->req.b_cmd);
		buf_append_c(&ctl->req.b_cmd, CMD_ARE_YOU_THERE);
		buf_append_c(&ctl->req.b_cmd, CMD_ARE_YOU_THERE | MH_BIT_MSB);
		err = dp_submit_request(ctl->dp, &ctl->req);
		if(err)
			return -1;
		dp_loop(ctl->dp, 1);

		switch(ctl->req.state) {
		case CRS_SUCCESS:
			success++;
			break;
		case CRS_TIMEOUT:
			success = 0;
			break;
		case CRS_ERROR:
			return -1;
			break;
		}
	}

	if(success < 3) {
		err("CTL init failed, device does not respond!");
		return -1;
	}

	/* Obtain the version information. */
	buf_reset(&ctl->req.b_cmd);
	buf_append_c(&ctl->req.b_cmd, CMD_GET_VERSION);
	buf_append_c(&ctl->req.b_cmd, CMD_GET_VERSION |MH_BIT_MSB);
	dp_submit_request(ctl->dp, &ctl->req);
	dp_loop(ctl->dp, 1);
	if(ctl->req.state != CRS_SUCCESS) {
		err("Failed to obtain device information! (state %d)", ctl->req.state);
		return -1;
	}
		
	parse_version(&ctl->mhi, &ctl->req);
	info("Detected microHam %s firmware %d.%d", 
	     ctl->mhi.type_str, ctl->mhi.ver_fw_major, 
	     ctl->mhi.ver_fw_minor);

#if 0
	/* Start VSPs */
	if(ctl->mhi.flags & MHF_HAS_CAT1) 
		start_vsp(ctl, "CAT1", MH_CHANNEL_R1);

	if(ctl->mhi.flags & MHF_HAS_WINKEY)
		start_vsp(ctl, "WK", MH_CHANNEL_WINKEY);

#endif

	apply_cfg(ctl);

	dp_submit_timer(ctl->dp, ctl->tmr_heartbeat);

	info("Keyer connected");

	return 0;
}

void ctl_destroy(struct ctl *ctl) {
	if(ctl) {
		vsp_destroy_all();
		if(ctl->tmr_heartbeat)
			dp_destroy_timer(ctl->tmr_heartbeat);
		free(ctl);
	}
}

const struct mh_info *ctl_get_mhinfo(struct ctl *ctl) {
	return &ctl->mhi;
}

static int apply_cfg(struct ctl *ctl) {
	struct buffer *b = &ctl->req.b_cmd;
	const char *devdir = cfg_get_str(ctl->cfg, "//Daemon/DevDir", "mhuxd");
	float f_baud;
	int i_baud;
	int rtscts;
	int databits;
	float stopbits;
	unsigned char c;

	if(ctl->mhi.flags & MHF_HAS_CAT1) {
		f_baud = cfg_get_int(ctl->cfg, "//Radio:1/BaudRate", 9600);
		databits = cfg_get_int(ctl->cfg, "//Radio:1/DataBits", 8);
		stopbits = cfg_get_float(ctl->cfg, "//Radio:1/StopBits", 1);
		rtscts   = cfg_get_bool(ctl->cfg, "//Radio:1/RtsCts", 0);

		info("CTL configure CAT1 baud: %f databits: %d stopbits %2.1f rtscts %d", f_baud, databits, stopbits, rtscts);

		if(stopbits == 1.5)
			stopbits = 3;

		buf_append_c(b, CMD_SET_CHANNEL_R1);
		i_baud = 11059200 / f_baud;
		buf_append_c(b, i_baud & 0xff);
		buf_append_c(b, i_baud >> 8);
		c = (((int)stopbits) -1) << 2;  /* bits 2 + 3 */
		c |= rtscts << 4;
		c |= (databits - 5) << 5; /* bits 5 + 6 */
		buf_append_c(b, c);

		if(mhi_has_long_cat_setting(ctl->mhi.type)) {
			buf_append_c(b, cfg_get_int(ctl->cfg, "//Radio:1/RadioType", 0xfe));
			buf_append_c(b, cfg_get_int(ctl->cfg, "//Radio:1/IcomAddress", 0x00));
			c = cfg_get_bool(ctl->cfg, "//Radio:1/IcomPW1Connected", 0) << 0;
			c |= cfg_get_bool(ctl->cfg, "//Radio:1/DigitalOverVoice", 0) << 1;
			c |= cfg_get_bool(ctl->cfg, "//Radio:1/UseDecoderIfConnected", 0) << 3;
			c |= cfg_get_bool(ctl->cfg, "//Radio:1/DontInterfereUSBControl", 0) << 4;
			buf_append_c(b, c);
		}
		buf_append_c(b, CMD_SET_CHANNEL_R1 | MH_BIT_MSB);
		dp_submit_request(ctl->dp, &ctl->req);
		dp_loop(ctl->dp, 1);
	}

	if(ctl->mhi.flags & MHF_HAS_CAT2) {
		f_baud = cfg_get_int(ctl->cfg, "//Radio:2/BaudRate", 9600);
		databits = cfg_get_int(ctl->cfg, "//Radio:2/DataBits", 8);
		stopbits = cfg_get_float(ctl->cfg, "//Radio:2/StopBits", 1);
		rtscts   = cfg_get_bool(ctl->cfg, "//Radio:2/RtsCts", 0);

		info("CTL configure CAT2 baud: %f databits: %d stopbits %2.1f rtscts %d", f_baud, databits, stopbits, rtscts);

		if(stopbits == 1.5)
			stopbits = 3;

		buf_append_c(b, CMD_SET_CHANNEL_AUX_R2);
		i_baud = 11059200 / f_baud;
		buf_append_c(b, i_baud & 0xff);
		buf_append_c(b, i_baud >> 8);
		c = (((int)stopbits) -1) << 2;  /* bits 2 + 3 */
		c |= rtscts << 4;
		c |= (databits - 5) << 5; /* bits 5 + 6 */
		buf_append_c(b, c);

		if(mhi_has_long_cat_setting(ctl->mhi.type)) {
			buf_append_c(b, cfg_get_int(ctl->cfg, "//Radio:2/RadioType", 0xfe));
			buf_append_c(b, cfg_get_int(ctl->cfg, "//Radio:2/IcomAddress", 0x00));
			c = cfg_get_bool(ctl->cfg, "//Radio:2/IcomPW1Connected", 0) << 0;
			c |= cfg_get_bool(ctl->cfg, "//Radio:2/DigitalOverVoice", 0) << 1;
			c |= cfg_get_bool(ctl->cfg, "//Radio:2/UseDecoderIfConnected", 0) << 3;
			c |= cfg_get_bool(ctl->cfg, "//Radio:2/DontInterfereUSBControl", 0) << 4;
			buf_append_c(b, c);
		}
		buf_append_c(b, CMD_SET_CHANNEL_AUX_R2 | MH_BIT_MSB);
		dp_submit_request(ctl->dp, &ctl->req);
		dp_loop(ctl->dp, 1);
	}

	if(ctl->mhi.flags & MHF_HAS_FSK1) {
		f_baud = cfg_get_float(ctl->cfg, "//Fsk:1/BaudRate", 9600);
		databits = cfg_get_int(ctl->cfg, "//Fsk:1/DataBits", 8);
		stopbits = cfg_get_float(ctl->cfg, "//Fsk:1/StopBits", 1);
		rtscts   = cfg_get_bool(ctl->cfg, "//Fsk:1/RtsCts", 0);

		info("CTL configure FSK1 baud: %f databits: %d stopbits %2.1f rtscts %d", f_baud, databits, stopbits, rtscts);

		if(stopbits == 1.5)
			stopbits = 3;

		buf_append_c(b, CMD_SET_CHANNEL_FSK_R1);
		i_baud = 2700 / f_baud;
		buf_append_c(b, i_baud & 0xff);
		buf_append_c(b, i_baud >> 8);
		c = (((int)stopbits) -1) << 2;  /* bits 2 + 3 */
		c |= rtscts << 4;
		c |= (databits - 5) << 5; /* bits 5 + 6 */
		buf_append_c(b, c);

		buf_append_c(b, CMD_SET_CHANNEL_FSK_R1 | MH_BIT_MSB);
		dp_submit_request(ctl->dp, &ctl->req);
		dp_loop(ctl->dp, 1);
	}

	if(ctl->mhi.flags & MHF_HAS_FSK2) {
		f_baud = cfg_get_float(ctl->cfg, "//Fsk:2/BaudRate", 9600);
		databits = cfg_get_int(ctl->cfg, "//Fsk:2/DataBits", 8);
		stopbits = cfg_get_float(ctl->cfg, "//Fsk:2/StopBits", 1);
		rtscts   = cfg_get_bool(ctl->cfg, "//Fsk:2/RtsCts", 0);

		info("CTL configure FSK2 baud: %f databits: %d stopbits %2.1f rtscts %d", f_baud, databits, stopbits, rtscts);

		if(stopbits == 1.5)
			stopbits = 3;

		buf_append_c(b, CMD_SET_CHANNEL_FSK_R2);
		i_baud = 2700 / f_baud;
		buf_append_c(b, i_baud & 0xff);
		buf_append_c(b, i_baud >> 8);
		c = (((int)stopbits) -1) << 2;  /* bits 2 + 3 */
		c |= rtscts << 4;
		c |= (databits - 5) << 5; /* bits 5 + 6 */
		buf_append_c(b, c);

		buf_append_c(b, CMD_SET_CHANNEL_FSK_R2 | MH_BIT_MSB);
		dp_submit_request(ctl->dp, &ctl->req);
		dp_loop(ctl->dp, 1);
	}


	if(cfg_get_bool(ctl->cfg, "//Radio:1/VspEnabled", 0 )) {
		if(!(ctl->mhi.flags & MHF_HAS_CAT1)) {
			err("CTL Error VSP enabled but CAT not supported for this keyer!");
		} else {
			struct vsp_config vcfg;
			memset(&vcfg, 0, sizeof(vcfg));
			snprintf(vcfg.devname, sizeof(vcfg.devname), "%s/cat1", devdir);

			if(cfg_get_bool(ctl->cfg, "//Radio:1/VspRtsIsPtt", 0))
				vcfg.flags |= VSPFL_RTS_IS_PTT;
			if(cfg_get_bool(ctl->cfg, "//Radio:1/VspDtrIsPtt", 0))
				vcfg.flags |= VSPFL_DTR_IS_PTT;

			start_vsp(ctl, &vcfg, MH_CHANNEL_R1);
		}
	}

	if(cfg_get_bool(ctl->cfg, "//Radio:2/VspEnabled", 0 )) {
		if(!(ctl->mhi.flags & MHF_HAS_CAT2)) {
			err("CTL Error VSP enabled but 2nd radio CAT not supported for this keyer!");
		} else {
			struct vsp_config vcfg;
			memset(&vcfg, 0, sizeof(vcfg));
			snprintf(vcfg.devname, sizeof(vcfg.devname), "%s/cat2", devdir);

			if(cfg_get_bool(ctl->cfg, "//Radio:2/VspRtsIsPtt", 0))
				vcfg.flags |= VSPFL_RTS_IS_PTT;
			if(cfg_get_bool(ctl->cfg, "//Radio:2/VspDtrIsPtt", 0))
				vcfg.flags |= VSPFL_DTR_IS_PTT;

			start_vsp(ctl, &vcfg, MH_CHANNEL_R2);
		}
	}

	if(cfg_get_str(ctl->cfg, "//Radio:1/VspPttEnabled", NULL )) {
		if(!(ctl->mhi.flags & MHF_HAS_CAT1)) {
			err("CTL Error VSP PTT enabled but CAT not supported for this keyer!");
		} else {
			struct vsp_config vcfg;
			memset(&vcfg, 0, sizeof(vcfg));
			snprintf(vcfg.devname, sizeof(vcfg.devname), "%s/ptt1", devdir);

			if(cfg_get_bool(ctl->cfg, "//Radio:1/VspPttRtsIsPtt", 0))
				vcfg.flags |= VSPFL_RTS_IS_PTT;
			if(cfg_get_bool(ctl->cfg, "//Radio:1/VspPttDtrIsPtt", 0))
				vcfg.flags |= VSPFL_DTR_IS_PTT;

			start_vsp_ptt(ctl, &vcfg, 1);
		}
	}

	if(cfg_get_str(ctl->cfg, "//Radio:2/VspPttEnabled", NULL )) {
		if(!(ctl->mhi.flags & MHF_HAS_CAT1)) {
			err("CTL Error VSP PTT enabled but 2nd radio CAT not supported for this keyer!");
		} else {
			struct vsp_config vcfg;
			memset(&vcfg, 0, sizeof(vcfg));
			snprintf(vcfg.devname, sizeof(vcfg.devname), "%s/ptt2", devdir);

			if(cfg_get_bool(ctl->cfg, "//Radio:2/VspPttRtsIsPtt", 0))
				vcfg.flags |= VSPFL_RTS_IS_PTT;
			if(cfg_get_bool(ctl->cfg, "//Radio:2/VspPttDtrIsPtt", 0))
				vcfg.flags |= VSPFL_DTR_IS_PTT;

			start_vsp_ptt(ctl, &vcfg, 2);
		}
	}


	if(cfg_get_str(ctl->cfg, "//Winkey/VspEnabled", NULL )) {
		if(!(ctl->mhi.flags & MHF_HAS_WINKEY)) {
			err("CTL Error VSP Winkey enabled but Winkey not supported for this keyer!");
		} else {
			struct vsp_config vcfg;
			memset(&vcfg, 0, sizeof(vcfg));
			snprintf(vcfg.devname, sizeof(vcfg.devname), "%s/wk1", devdir);

			if(cfg_get_bool(ctl->cfg, "//Winkey/VspRtsIsPtt", 0))
				vcfg.flags |= VSPFL_RTS_IS_PTT;
			if(cfg_get_bool(ctl->cfg, "//Winkey/VspDtrIsPtt", 0))
				vcfg.flags |= VSPFL_DTR_IS_PTT;

			start_vsp(ctl, &vcfg, MH_CHANNEL_WINKEY);
		}
	}

	if(cfg_get_str(ctl->cfg, "//Fsk:1/VspEnabled", NULL )) {
		if(!(ctl->mhi.flags & MHF_HAS_FSK1)) {
			err("CTL Error VSP FSK enabled but FSK not supported for this keyer!");
		} else {

			struct vsp_config vcfg;
			memset(&vcfg, 0, sizeof(vcfg));
			snprintf(vcfg.devname, sizeof(vcfg.devname), "%s/fsk1", devdir);

			if(cfg_get_bool(ctl->cfg, "//Fsk:1/VspRtsIsPtt", 0))
				vcfg.flags |= VSPFL_RTS_IS_PTT;
			if(cfg_get_bool(ctl->cfg, "//Fsk:1/VspDtrIsPtt", 0))
				vcfg.flags |= VSPFL_DTR_IS_PTT;

			start_vsp(ctl, &vcfg, MH_CHANNEL_R1_FSK);
		}
	}

	if(cfg_get_str(ctl->cfg, "//Fsk:2/VspEnabled", NULL )) {
		if(!(ctl->mhi.flags & MHF_HAS_FSK2)) {
			err("CTL Error VSP FSK enabled on 2nd radio but not supported for this keyer!");
		} else {
			struct vsp_config vcfg;
			memset(&vcfg, 0, sizeof(vcfg));
			snprintf(vcfg.devname, sizeof(vcfg.devname), "%s/fsk2", devdir);

			if(cfg_get_bool(ctl->cfg, "//Fsk:1/VspRtsIsPtt", 0))
				vcfg.flags |= VSPFL_RTS_IS_PTT;
			if(cfg_get_bool(ctl->cfg, "//Fsk:1/VspDtrIsPtt", 0))
				vcfg.flags |= VSPFL_DTR_IS_PTT;

			start_vsp(ctl, &vcfg, MH_CHANNEL_R1_FSK);
		}
	}


	struct kcfg *kcfg = kcfg_create(&ctl->mhi);
	struct buffer *kb;

	if(!kcfg)
		return -1;

	info("CTL Configure keyer");

	kcfg_apply_cfg(kcfg, ctl->cfg);

	kb = kcfg_get_buffer(kcfg);
	buf_append_c(b, CMD_SET_SETTINGS);
	buf_append(b, kb->data, kb->size);
	buf_append_c(b, CMD_SET_SETTINGS | MH_BIT_MSB);
	dp_submit_request(ctl->dp, &ctl->req);
	dp_loop(ctl->dp, 1);

	kcfg_destroy(kcfg);

	return 0;
}

void ctl_set_cfg(struct ctl *ctl, struct config *cfg) {
	struct cmd_request *req = calloc(1, sizeof(*req));
	struct kcfg *kcfg = kcfg_create(&ctl->mhi);

	if(!req)
		return;

	if(!kcfg)
		return;

	ctl->cfg = cfg;

	req->data = ctl;
	req->req_done = req_done_dealloc;

	struct buffer *b = &req->b_cmd;

	struct buffer *kb;


	info("CTL Configure keyer");

	kcfg_apply_cfg(kcfg, ctl->cfg);

	kb = kcfg_get_buffer(kcfg);
	buf_append_c(b, CMD_SET_SETTINGS);
	buf_append(b, kb->data, kb->size);
	buf_append_c(b, CMD_SET_SETTINGS | MH_BIT_MSB);
	dp_submit_request(ctl->dp, req);
	kcfg_destroy(kcfg);
}
