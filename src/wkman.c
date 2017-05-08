/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ev.h>
#include "clearsilver/util/neo_hdf.h"
#include "wkman.h"
#include "util.h"
#include "citem.h"
#include "devmgr.h"
#include "channel.h"
#include "mhrouter.h"
#include "mhcontrol.h"
#include "cfgnod.h"
#include "logger.h"

#define MOD_ID "wkm"

#define WK_CFG_SIZE (15)

enum {
	WKM_STATE_OFFLINE,
	WKM_STATE_HOST_CLOSED,
	WKM_STATE_HOST_OPEN,
	WKM_STATE_HOST_OPEN_SENT,
};

enum {
	WKM_TASK_NONE,
	WKM_TASK_RESET,
	WKM_TAKS_READ_CFG,
	WKM_TAKS_WRITE_CFG,
};

struct wkman {
	struct device *dev;
	struct ev_loop *loop;
	ev_timer timeout_timer;
	struct mhc_keyer_state_callback *kscb;
	uint8_t cfg[WK_CFG_SIZE];
	uint8_t read_size;
	uint8_t state;
	uint8_t task;
	uint8_t version;
};

struct citem citems[] = {
	// Mode Register
	CITEMD("disablePaddleWatchdog", 0, 7, 1, 1 ),
	CITEMD("paddleEchoBack",        0, 6, 1, 0 ),
	CITEMD("keyMode",               0, 5, 2, 0 ),
	CITEMD("paddleSwap",            0, 3, 1, 0 ),
	CITEMD("serialEchoBack",        0, 2, 1, 1 ),
	CITEMD("autoSpace",             0, 1, 1, 0 ),
	CITEMD("ctSpacing",             0, 0, 1, 0 ),

	CITEMD("speedWpm",              1, 7, 8, 0 ),
	CITEMD("sideToneFreq",          2, 7, 8, 5 ),
	CITEMD("weight",                3, 7, 8, 50 ),
	CITEMD("leadInTime",            4, 7, 8, 10 ),
	CITEMD("tailTime",              5, 7, 8, 10 ),
	CITEMD("minWpm",                6, 7, 8, 10 ),
	CITEMD("wpmRange",              7, 7, 8, 50 ),
	CITEMD("1stExtension",          8, 7, 8, 0 ),
	CITEMD("keyComp",               9, 7, 8, 0 ),
	CITEMD("farnsWpm",             10, 7, 8, 0 ),
	CITEMD("paddleSetpoint",       11, 7, 8, 50 ),
	CITEMD("ditDahRatio",          12, 7, 8, 50 ),
	CITEMD("pinConfig",            13, 3, 4, 5 ),
	CITEMD("hangTime",             13, 5, 2, 0 ),
	CITEMD("ultimaticMode",        13, 7, 2, 0 ),
	CITEMD("potRange",             14, 7, 8, 255 )
};

int wkm_set_value(struct wkman *wkman, const char *key, uint8_t val) {
	int r = citem_set_value(citems, ARRAY_SIZE(citems), wkman->cfg, sizeof(wkman->cfg), key, val);
	if(r == -1) {
		err("(wkman) could not set value %s = %d", key, val);
	}
	return r;
}

void read_cb(struct mh_router *router, unsigned const char *data , int len, int channel, void *user_data) {
	(void)router; (void)channel;
	struct wkman *wkman = user_data;
	int err;

	dbg1("%s %s() %d", wkman->dev->serial, __func__, len);
	dbg1_h(wkman->dev->serial, "fm k", data, len);

	switch(wkman->state) {
	case WKM_STATE_HOST_OPEN_SENT:
		wkman->version = *data;
		wkman->state = WKM_STATE_HOST_OPEN;
		ev_timer_stop(wkman->loop, &wkman->timeout_timer);
		err = wkm_write_cfg(wkman);
		if(err != WKM_RESULT_OK)
			err("(wkman) could not send config to winkey (%s)!", wkm_err_string(err));
#if 0
		err = wkm_host_close(wkman);
		if(err != WKM_RESULT_OK)
			err("(wkman) could not send HOST CLOSE to winkey (%s)!", wkm_err_string(err));
#endif
		break;
	}
}

static void timeout_cb (struct ev_loop *loop,  struct ev_timer *w, int revents) {
	(void)loop; (void)revents;
	struct wkman *wkman = w->data;

	switch(wkman->state) {
	case WKM_STATE_HOST_OPEN_SENT:
		err("(wkman) winkey host_open failed (timeout)!");
		mhr_rem_consumer_cb(wkman->dev->router, read_cb, MH_CHANNEL_WINKEY);
		wkman->state = WKM_STATE_HOST_CLOSED;
		break;
	default:
		err("(wkman) timeout occurred in unexpected state %d!", wkman->state);
		break;
	}
}

static void keyer_state_changed_cb(const char *serial, int state, void *user_data) {
	(void)state; (void)serial;
	struct wkman *wkman = user_data;

	dbg1("%s %s() %s", wkman->dev->serial, __func__, mhc_state_str(state));


	if(state == MHC_KEYER_STATE_ONLINE) {
		int err;
		wkman->state = WKM_STATE_HOST_CLOSED;

		err = wkm_reset(wkman);
		if(WKM_RESULT_OK != err) {
			err("(wkman) could not send reset command to winkey!");
			return;
		}
#if 0
		err = wkm_host_open(wkman);
		if(WKM_RESULT_OK != err) {
			err("(wkman) could not send HOST OPEN command to winkey (%s)!", wkm_err_string(err));
			return;
		}

		wkman->state = WKM_STATE_HOST_OPEN_SENT;
#else
		wkman->state = WKM_STATE_HOST_CLOSED;
		err = wkm_write_cfg(wkman);
		if(err != WKM_RESULT_OK)
			err("(wkman) could not send config to winkey (%s)!", wkm_err_string(err));

#endif
	} else {
		wkman->state = WKM_STATE_OFFLINE;
	}
}

struct wkman *wkm_create(struct ev_loop *loop, struct device *dev) {
	struct wkman *wkman = w_calloc(1, sizeof(*wkman));
	uint16_t i;

	dbg1("%s %s()", dev->serial, __func__);

	wkman->dev = dev;
	wkman->loop = loop;


	for(i = 0; i < ARRAY_SIZE(citems); i++) {
		wkm_set_value(wkman, citems[i].key, citems[i].def);
	}

	ev_timer_init(&wkman->timeout_timer, timeout_cb, 2, 0.);
	wkman->timeout_timer.data = wkman;

	wkman->state = mhc_is_online(wkman->dev->ctl) ? WKM_STATE_HOST_CLOSED : WKM_STATE_OFFLINE;

	wkman->kscb = mhc_add_keyer_state_changed_cb(dev->ctl, keyer_state_changed_cb, wkman);

	mhr_add_consumer_cb(wkman->dev->router, read_cb, MH_CHANNEL_WINKEY, wkman);

	return wkman;
}

void wkm_destroy(struct wkman *wkman) {
	if(!wkman)
		return;
	ev_timer_stop(wkman->loop, &wkman->timeout_timer);
	mhr_rem_consumer_cb(wkman->dev->router, read_cb, MH_CHANNEL_WINKEY);
	mhc_rem_keyer_state_changed_cb(wkman->dev->ctl, wkman->kscb);
	free(wkman);
}

int wkm_opts_to_cfg(struct wkman *wkman, struct cfg *cfg) {
	return citems_to_cfg(cfg, citems, ARRAY_SIZE(citems), wkman->cfg, sizeof(wkman->cfg));
}

int wkm_cfg_to_opts(struct wkman *wkman, struct cfg *cfg) {
	HDF *base_hdf = (HDF*)cfg;
	HDF *hdf;
	int rval = 0;
	dbg1("%s %s()", wkman->dev->serial, __func__);
	for(hdf = hdf_obj_child(base_hdf); hdf; hdf = hdf_obj_next(hdf)) {
		const char *key = hdf_obj_name(hdf);
		const char *val_str = hdf_obj_value(hdf);
		int val = atoi(val_str);
		rval += wkm_set_value(wkman, key, val);
	}
	return rval;
}

int wkm_reset(struct wkman *wkman) {
	const uint8_t cmd[] = { 0x00, 0x01 };
	int len = sizeof(cmd);

	dbg1("%s %s()", wkman->dev->serial,  __func__);
	
	if(!mhc_is_online(wkman->dev->ctl))
		return WKM_RESULT_DEVICE_OFFLINE;

	if(len != mhr_send_in(wkman->dev->router, cmd, len, MH_CHANNEL_WINKEY))
		return WKM_RESULT_IO_ERROR;

	wkman->state = WKM_STATE_HOST_CLOSED;

	return WKM_RESULT_OK;
}

int wkm_host_open(struct wkman *wkman) {
	const uint8_t cmd[] = { 0x00, 0x02 };
	int len = sizeof(cmd);

	dbg1("%s %s()", wkman->dev->serial,  __func__);

	if(!mhc_is_online(wkman->dev->ctl))
		return WKM_RESULT_DEVICE_OFFLINE;

	if(wkman->state != WKM_STATE_HOST_CLOSED && wkman->state != WKM_STATE_HOST_OPEN) {
		return WKM_RESULT_BUSY;
	}

	if(len != mhr_send_in(wkman->dev->router, cmd, len, MH_CHANNEL_WINKEY))
		return WKM_RESULT_IO_ERROR;

	ev_timer_start(wkman->loop, &wkman->timeout_timer);

	wkman->state = WKM_STATE_HOST_OPEN_SENT;

	return WKM_RESULT_OK;
}

int wkm_host_close(struct wkman *wkman) {
	const uint8_t cmd[] = { 0x00, 0x03 };
	int len = sizeof(cmd);

	if(!mhc_is_online(wkman->dev->ctl))
		return WKM_RESULT_DEVICE_OFFLINE;

	if(wkman->state != WKM_STATE_HOST_CLOSED && wkman->state != WKM_STATE_HOST_OPEN) {
		return WKM_RESULT_BUSY;
	}

	if(len != mhr_send_in(wkman->dev->router, cmd, len, MH_CHANNEL_WINKEY))
		return WKM_RESULT_IO_ERROR;

	wkman->state = WKM_STATE_HOST_CLOSED;

	return WKM_RESULT_OK;
}

int wkm_write_cfg(struct wkman *wkman) {
	uint8_t cmd[1 + sizeof(wkman->cfg)] = { 0x0f };
	int len = sizeof(cmd);

	dbg1("%s %s()", wkman->dev->serial, __func__);

	if(!mhc_is_online(wkman->dev->ctl))
		return WKM_RESULT_DEVICE_OFFLINE;

	if(wkman->state != WKM_STATE_HOST_OPEN && wkman->state != WKM_STATE_HOST_CLOSED) {
		return WKM_RESULT_BUSY;
	}

	memcpy(cmd + 1, wkman->cfg, sizeof(wkman->cfg));

	if(len != mhr_send_in(wkman->dev->router, cmd, len, MH_CHANNEL_WINKEY))
		return WKM_RESULT_IO_ERROR;

	return WKM_RESULT_OK;
}

const char *wkm_err_string(int error) { 
	switch(error) {
	case WKM_RESULT_OK:
		return "success";
	case WKM_RESULT_DEVICE_OFFLINE:
		return "device offline";
	case WKM_RESULT_TIMEOUT:
		return "timeout";
 	case WKM_RESULT_IO_ERROR:
		return "i/o error";
	case WKM_RESULT_BUSY:
		return "winkey busy";
	default:
		return "unkown error";
	}
}

