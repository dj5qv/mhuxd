/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <ev.h>
#include "mhrouter.h"
#include "mhflags.h"
#include "util.h"
#include "pglist.h"
#include "buffer.h"
#include "logger.h"
#include "channel.h"
#include "mux.h"
#include "demux.h"

#define MOD_ID "mhr"

#define LB_BYTES_PER_IVAL (20.0)
#define LB_CARRY_OVER (2)

struct Producer {
	struct PGNode node;
	struct mh_router *router;
	int fd;
	ev_io w;
	int channel;
};

struct Consumer {
	struct PGNode node;
	struct mh_router *router;
	int fd;
	ev_io w;
	struct buffer buf;
	int channel;
};

struct ConsumerCb {
	struct PGNode node;
	struct mh_router *router;
	MHRConsumerCallback callback;
	struct buffer buf;
	int channel;
	void *user_data;
};

struct StatusCb {
	struct PGNode node;
	struct mh_router *router;
	MHRStatusCallback callback;
	void *user_data;
};

struct ProcessorCb {
	struct PGNode node;
	struct mh_router *router;
	MHRProcessorCallback callback;
	int channel;
	void *user_data;
};

struct LeakyBucket {
	float bps;
	double ival;
	double avail_this_ival;
	ev_timer timer;
	struct mh_router *router;
	int channel;
};

struct mh_router {
	struct PGList consumer_list[ALL_NUM_CHANNELS];
	struct PGList producer_list[ALL_NUM_CHANNELS];
	struct PGList consumer_cb_list[ALL_NUM_CHANNELS];
	struct PGList processor_cb_list[ALL_NUM_CHANNELS];
	struct PGList status_cb_list;
	struct buffer channel_buf_out[ALL_NUM_CHANNELS];
	struct LeakyBucket lb[ALL_NUM_CHANNELS];
	int fd;
	struct ev_loop *loop;
	struct buffer buf_in;
	struct buffer buf_out;
	ev_io w_in, w_out;
	struct dmx *dmx;
	char *serial;
	uint8_t rflag[2];
	uint8_t wflag;
	uint8_t has_flags_channel;
};

static void lb_cb(struct ev_loop *loop,  struct ev_timer *w, int revents) {
	(void)loop; (void)revents;
	struct LeakyBucket *lb = w->data;
	struct mh_router *router = lb->router;
	struct Producer *prd;
	int channel = lb - router->lb;

	dbg1("%s %s() %s %f", router->serial, __func__, ch_channel2str(channel), lb->avail_this_ival);

	if(lb->avail_this_ival == LB_BYTES_PER_IVAL + LB_CARRY_OVER) {
		// No data during previous ival, stop timer.
		ev_timer_stop(router->loop, &lb->timer);
		return;
	}

	lb->avail_this_ival = LB_BYTES_PER_IVAL + (lb->avail_this_ival > LB_CARRY_OVER ? LB_CARRY_OVER : lb->avail_this_ival);

	PG_SCANLIST(&router->producer_list[channel], prd)
			ev_io_start(router->loop, &prd->w);
}

/*
 * Set maximum channel throughput in bytes per second.
 * Set bps = 0 to disable.
 */
void mhr_set_bps_limit(struct mh_router *router, int channel, float bps) {
	struct LeakyBucket *lb = &router->lb[channel];

	if(bps == lb->bps)
		return;

	if(bps == 0) {
		if(lb->bps) {
			ev_timer_stop(router->loop, &lb->timer);
			lb->bps = 0;
		}
		return;
	}

	if(lb->bps)
		ev_timer_stop(router->loop, &lb->timer);

	lb->bps = bps;
	lb->router = router;
	lb->ival = (1 / bps) * LB_BYTES_PER_IVAL;
	lb->avail_this_ival = LB_BYTES_PER_IVAL + LB_CARRY_OVER;
	ev_timer_init(&lb->timer, lb_cb, 0., lb->ival);
	lb->timer.data = lb;
	ev_timer_start(router->loop, &lb->timer);

	dbg1("%s %s() %s bytes/sec: %f ival: %f", router->serial, __func__, ch_channel2str(channel), bps, lb->ival);
}

static void switch_producer_events(struct mh_router *router, int active) {
	struct Producer *prd;
	int i;
	void (*set_func)(struct ev_loop *, struct ev_io *) =
		active ? ev_io_start : ev_io_stop;

	for(i=0; i<ALL_NUM_CHANNELS; i++) {
		PG_SCANLIST(&router->producer_list[i], prd) {
			set_func(router->loop, &prd->w);
		}
	}
}

static int is_ptt_channel(int channel) {
	return (channel == CH_PTT1 || channel == CH_PTT2);
}

static void process_in_flags(struct mh_router *router, int c) {
	struct Consumer *cns;
	struct ConsumerCb *cnc;
	int ptt_ch = c & MHD2CFL_R2 ? CH_PTT2 : CH_PTT1;
	uint8_t idx = (ptt_ch == CH_PTT1) ? 0 : 1;

	if( (router->rflag[idx] & MHD2CFL_ANY_PTT) != (c & MHD2CFL_ANY_PTT) ) {
		PG_SCANLIST(&router->consumer_list[ptt_ch], cns) {
			// no need to keep outdated values
			buf_reset(&cns->buf);
			buf_append_c(&cns->buf, (c & MHD2CFL_ANY_PTT) ? '1' : '0');
			ev_io_start(router->loop, &cns->w);
		}

		PG_SCANLIST(&router->consumer_cb_list[ptt_ch], cnc) {
			uint8_t b = (c & MHD2CFL_ANY_PTT) ? '1' : '0';
			cnc->callback(router, &b, 1, ptt_ch, cnc->user_data);
		}
	}
}

static void process_ptt_producer(struct Producer *prd, struct buffer *b) {
	struct mh_router *router = prd->router;
	uint8_t newflag = router->wflag;
	int r;
	int c;
	int16_t push = 0;

	dbg1("%s %s()", router->serial, __func__);

	if(!b->size)
		return;

	while(-1 != (c = buf_get_c(b))) {

		if( c == '1' ) {
			dbg1("%s %s() ptt on", router->serial, __func__);
			newflag |= (prd->channel == CH_PTT1) ? MHC2DFL_PTT_R1 : MHC2DFL_PTT_R2;
			push = 1;
		}

		if( c == '0' ) {
			dbg1("%s %s() ptt off", router->serial, __func__);
			newflag &= (prd->channel == CH_PTT1) ? ~MHC2DFL_PTT_R1 : ~MHC2DFL_PTT_R2;
			push = 1;
		}
	}	      

	if(push) {
		r = mhr_send(router, &newflag, 1, MH_CHANNEL_FLAGS);
		if(r != 1)
			err("(mhr) error sending data to flags channel!");
	}
}

static void producer_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	struct mh_router *router;
	struct Producer *prd;
	struct ProcessorCb *prc;
	struct buffer *b;
	int r, avail;
	prd = w->data;
	router = prd->router;

	dbg1("%s %s() %s", router->serial, __func__, ch_channel2str(prd->channel));

	if(!(revents & EV_READ))
		return;

	if(router->fd == -1)
		return;

	b = &router->channel_buf_out[prd->channel];

	avail = buf_size_avail(b);

	// leaky bucket
	if(router->lb[prd->channel].bps && avail > router->lb[prd->channel].avail_this_ival)
		avail = router->lb[prd->channel].avail_this_ival;

	if(!avail) {
		//		dbg1("%s avail 0", router->serial);
		ev_io_stop(loop, &prd->w);
		return;
	}

	r = read(prd->fd, b->data + b->size, avail);

	if(r > 0) {
		buf_add_size(b, r);

		if(router->lb[prd->channel].bps) {
			// Timer may have been stopped because of an idle ival
			ev_timer_start(router->loop, &router->lb[prd->channel].timer);
		}

		if(is_ptt_channel(prd->channel))
			process_ptt_producer(prd, b);

		PG_SCANLIST(&router->processor_cb_list[prd->channel], prc) {
			prc->callback(router, prd->channel, b, prd->fd, prc->user_data);
		}

		ev_io_start(loop, &router->w_out);

		if(router->lb[prd->channel].bps)
			router->lb[prd->channel].avail_this_ival -= r;
	}

	if(r == 0) {
		dbg1("%s connection %d closed by remote", router->serial, w->fd);
		mhr_rem_producer(router, prd->fd, prd->channel);
	}

	if(r < 0 && errno != EAGAIN) {
		err_e(errno, "(mhr) Error reading from producer %d, channel %d!", prd->fd, prd->channel);
		mhr_rem_producer(router, prd->fd, prd->channel);
		return;
	}
}

static void consumer_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	struct mh_router *router;
	struct Consumer *cns;
	struct buffer *b;
	int r;
	cns = w->data;
	router = cns->router;

	if(!(revents & EV_WRITE))
		return;

	b = &cns->buf;
	r = write(cns->fd, b->data + b->rpos, b->size - b->rpos);
	if(r > 0) 
		buf_consume(b, r);

	if(r < 0 && errno != EAGAIN) {
		err_e(errno, "(mhr) Error writing to fd %d, channel %d!", cns->fd, cns->channel);
		mhr_rem_consumer(router, cns->fd, cns->channel);
		return;
	}

	if(!b->size)
		ev_io_stop(loop, &cns->w);
}


static void keyer_in_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	struct mh_router *router;
	int r, c, channel, avail;
	router = w->data;
	if(!(revents & EV_READ))
		return;

	if(router->fd == -1)
		return;

	avail = buf_size_avail(&router->buf_in);

	r = read(router->fd, router->buf_in.data + router->buf_in.size, avail);
	// dbg1("%s, %s %d", router->serial, __func__, r);
	if(r > 0) {
		dbg1_h(router->serial, "fm k", router->buf_in.data + router->buf_in.size, r);
		buf_add_size(&router->buf_in, r);
	}

	if((r < 0 && errno != EAGAIN) || r == 0) {
		err_e(errno, "(mhr) Error reading from keyer!");
		mhr_set_keyer_fd(router, -1);
	}

	while(-1 != (c = buf_get_c(&router->buf_in))) {
		struct Consumer *cns;
		struct ConsumerCb *cnc;

		channel = dmx_demux(router->dmx, c);
		if(channel < 0 || channel >= MH_NUM_CHANNELS)
			continue;

		if(channel == MH_CHANNEL_FLAGS && router->has_flags_channel)
			process_in_flags(router, router->dmx->result_byte);

		PG_SCANLIST(&router->consumer_list[channel], cns) {
			if(channel == MH_CHANNEL_CONTROL) {
				int r = buf_append(&cns->buf, router->dmx->cmd_buffer, router->dmx->cmd_length);
				if(r != router->dmx->cmd_length)
					err("(mhr) Buffer overflow command channel!");
			} else {
				if(buf_append_c(&cns->buf, router->dmx->result_byte))
					err("(mhr) Buffer overflow channel %d!", channel);
			}
			ev_io_start(loop, &cns->w);
		}

		PG_SCANLIST(&router->consumer_cb_list[channel], cnc) {
			if(channel == MH_CHANNEL_CONTROL) 
				cnc->callback(router, router->dmx->cmd_buffer, router->dmx->cmd_length, channel, cnc->user_data);
			else 
				cnc->callback(router, &router->dmx->result_byte, 1, channel, cnc->user_data);

		}
	}
}

static void keyer_out_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	struct mh_router *router;
	int i, r;
	struct buffer *b;
	router = w->data;
	if(!(revents & EV_WRITE))
		return;

	if(router->fd == -1)
		return;

	b = &router->buf_out;

	mx_mux(&router->buf_out, router->channel_buf_out);

	r = write(router->fd, b->data + b->rpos, b->size - b->rpos);
	if(r > 0) {
		dbg1_h(router->serial, "to k", b->data + b->rpos, r);
		buf_consume(b, r);
		switch_producer_events(router, 1); // FIXME, room for optimization
	}

	if(r < 0 && errno != EAGAIN) {
		err_e(errno, "(mhr) Error writing to keyer!");
		mhr_set_keyer_fd(router, -1);
		return;
	}

	// FIXME Really should loop here until either all buffers are empty
	//       or write() returns 0.

	int need_to_write = 0;

	if(b->size)
		need_to_write++;
		
	for(i=0; i<MH_NUM_CHANNELS; i++) {
		if(router->channel_buf_out[i].size) {
			need_to_write++;
			break;
		}
	}

	if(!need_to_write)
		ev_io_stop(loop, &router->w_out);
}

struct mh_router *mhr_create(struct ev_loop *loop, const char *serial, uint8_t has_flags_channel) {
	struct mh_router *router;
	int i;

	dbg1("%s %s()", serial, __func__);

	router = w_calloc(1, sizeof(*router));
	router->has_flags_channel = has_flags_channel;
	router->serial = w_malloc(strlen(serial)+1);
	strcpy(router->serial, serial);
	router->fd = -1;
	router->loop = loop;
	ev_init(&router->w_in, keyer_in_cb);
	ev_init(&router->w_out, keyer_out_cb);
	router->w_in.data = router;
	router->w_out.data = router;
	router->dmx = dmx_create();

	for(i=0; i<ALL_NUM_CHANNELS; i++) {
		PG_NewList(&router->consumer_list[i]);
		PG_NewList(&router->producer_list[i]);
		PG_NewList(&router->consumer_cb_list[i]);
		PG_NewList(&router->processor_cb_list[i]);
	}

	PG_NewList(&router->status_cb_list);

	return router;
}

void mhr_destroy(struct mh_router *router) {
	struct Consumer *cns;
	struct Producer *prd;
	struct ConsumerCb *cnc;
	struct StatusCb *stc;
	int i;

	dbg1("%s %s()", router->serial, __func__);

	for(i=0; i<ALL_NUM_CHANNELS; i++) {
		if(router->lb[i].bps)
			ev_timer_stop(router->loop, &router->lb[i].timer);

		while((cns = (void *)PG_FIRSTENTRY(&router->consumer_list[i]))) {
			mhr_rem_consumer(router, cns->fd, i);
		}
		while((prd = (void *)PG_FIRSTENTRY(&router->producer_list[i]))) {
			mhr_rem_producer(router, prd->fd, i);
		}
		while((cnc = (void *)PG_FIRSTENTRY(&router->consumer_cb_list[i]))) {
			mhr_rem_consumer_cb(router, cnc->callback, i);
		}
	}

	while((stc = (void *)PG_FIRSTENTRY(&router->status_cb_list))) {
		mhr_rem_status_cb(router, stc->callback);
	}

	if(router->dmx)
		dmx_destroy(router->dmx);

	mhr_set_keyer_fd(router, -1);

	if(router->serial)
		free(router->serial);

	free(router);
}

void mhr_set_keyer_fd(struct mh_router *router, int fd) {
	if(fd == router->fd)
		return;

	dbg1("%s %s() %d", router->serial, __func__, fd);

	if(router->fd != -1) {
		ev_io_stop(router->loop, &router->w_in);
		ev_io_stop(router->loop, &router->w_out);
		close(router->fd);
	}

	router->fd = fd;

	if(fd != -1) {
		ev_io_init(&router->w_in, keyer_in_cb, fd, EV_READ);
		ev_io_init(&router->w_out, keyer_out_cb, fd, EV_WRITE);
		ev_io_start(router->loop, &router->w_in);
		// ev_io_start(router->loop, &router->w_out);
	}

	switch_producer_events(router, fd != -1);

	struct StatusCb *stc;
	PG_SCANLIST(&router->status_cb_list, stc) {
		if(stc->callback)
			stc->callback(router, fd != -1 ? MHROUTER_CONNECTED : MHROUTER_DISCONNECTED, stc->user_data);
	}
}

void mhr_add_consumer(struct mh_router *router, int fd, int channel) {
	struct Consumer *cns;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	if(channel == MH_CHANNEL_FLAGS && !router->has_flags_channel)
		return;

	if(fd == -1)
		return;

	dbg1("%s %s() %d %s", router->serial, __func__, fd, ch_channel2str(channel));

	cns = w_calloc(1, sizeof(*cns));
	cns->fd = fd;
	ev_io_init(&cns->w, consumer_cb, cns->fd, EV_WRITE);
	cns->w.data = cns;
	cns->router = router;
	cns->channel = channel;
	PG_AddTail(&router->consumer_list[channel], &cns->node);
}

void mhr_add_producer(struct mh_router *router, int fd, int channel) {
	struct Producer *prd;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	if(channel == MH_CHANNEL_FLAGS && !router->has_flags_channel)
		return;

	if(fd == -1)
		return;

	dbg1("%s %s() %d %s", router->serial, __func__, fd, ch_channel2str(channel));

	prd = w_calloc(1, sizeof(*prd));
	prd->fd = fd;
	ev_io_init(&prd->w, producer_cb, prd->fd, EV_READ);
	prd->w.data = prd;
	prd->router = router;
	prd->channel = channel;
	PG_AddTail(&router->producer_list[channel], &prd->node);
	if(router->fd != -1) {
		ev_io_start(router->loop, &prd->w);
	}
}

void mhr_add_consumer_cb(struct mh_router *router, MHRConsumerCallback callback, int channel,
			 void *user_data) {
	struct ConsumerCb *cnc;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	if(channel == MH_CHANNEL_FLAGS && !router->has_flags_channel)
		return;

	dbg1("%s %s() %s", router->serial, __func__, ch_channel2str(channel));

	cnc = w_calloc(1, sizeof(*cnc));
	cnc->callback = callback;
	cnc->router = router;
	cnc->channel = channel;
	cnc->user_data = user_data;
	PG_AddTail(&router->consumer_cb_list[channel], &cnc->node);
}

void mhr_add_status_cb(struct mh_router *router, MHRStatusCallback callback, void *user_data) {
	struct StatusCb *stc;

	dbg1("%s %s()", router->serial, __func__);
	
	stc = w_calloc(1, sizeof(*stc));
	stc->callback = callback;
	stc->router = router;
	stc->user_data = user_data;

	PG_AddTail(&router->status_cb_list, &stc->node);
}

void mhr_add_processor_cb(struct mh_router *router, MHRProcessorCallback callback, int channel, void *user_data) {
	struct ProcessorCb *prc;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	if(channel == MH_CHANNEL_FLAGS && !router->has_flags_channel)
		return;

	dbg1("%s %s()", router->serial, __func__);

	prc = w_calloc(1, sizeof(*prc));
	prc->callback = callback;
	prc->router = router;
	prc->channel = channel;
	prc->user_data = user_data;
	PG_AddTail(&router->processor_cb_list[channel], &prc->node);
}

void mhr_rem_consumer(struct mh_router *router, int fd, int channel) {
	struct Consumer *cns;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	dbg1("%s %s() %d %s", router->serial, __func__, fd, ch_channel2str(channel));

	PG_SCANLIST(&router->consumer_list[channel], cns) {
		if(cns->fd == fd) {
			PG_Remove(&cns->node);
			ev_io_stop(router->loop, &cns->w);
			close(cns->fd);
			free(cns);
			return;
		}
	}

	warn("(mhr) %s() fd not found", __func__);
}

void mhr_rem_producer(struct mh_router *router, int fd, int channel) {
	struct Producer *prd;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	dbg1("%s %s() %d %s", router->serial, __func__, fd, ch_channel2str(channel));

	PG_SCANLIST(&router->producer_list[channel], prd) {
		if(prd->fd == fd) {
			PG_Remove(&prd->node);
			ev_io_stop(router->loop, &prd->w);
			close(prd->fd);
			free(prd);
			return;
		}
	}

	warn("(mhr) %s() fd not found", __func__);
}

void mhr_rem_consumer_cb(struct mh_router *router, MHRConsumerCallback callback, int channel) {
	struct ConsumerCb *cnc;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	dbg1("%s %s() %s", router->serial, __func__, ch_channel2str(channel));

	PG_SCANLIST(&router->consumer_cb_list[channel], cnc) {
		if(cnc->callback == callback) {
			PG_Remove(&cnc->node);
			free(cnc);
			return;
		}
	}
	warn("(mhr) %s() callback not found", __func__);
}

void mhr_rem_status_cb(struct mh_router *router, MHRStatusCallback callback) {
	struct StatusCb *stc;

	dbg1("%s %s()",router->serial,  __func__);

	PG_SCANLIST(&router->status_cb_list, stc) {
		if(stc->callback == callback) {
			PG_Remove(&stc->node);
			free(stc);
			return;
		}
	}
	warn("(mhr) %s() callback not found", __func__);
}

void mhr_rem_processor_cb(struct mh_router *router, MHRProcessorCallback callback, int channel) {
	struct ProcessorCb *prc;

	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return;

	dbg1("%s %s()", router->serial, __func__);

	PG_SCANLIST(&router->processor_cb_list[channel], prc) {
		if(prc->callback == callback) {
			PG_Remove(&prc->node);
			free(prc);
			return;
		}
	}

	warn("(mhr) %s() callback not found", __func__);
}

int mhr_send(struct mh_router *router, const uint8_t *data, unsigned int len, int channel) {
	if(channel < 0 || channel >= ALL_NUM_CHANNELS)
		return -1;

	if(channel == MH_CHANNEL_FLAGS && !router->has_flags_channel) {
		warn("%s() %s write to flags channel not supported on this device!", __func__, router->serial);
		return -1;
	}

	if(len > BUFFER_CAPACITY)
		return -1;
	if(router->fd == -1)
		return -1;

	if(len > buf_size_avail(&router->channel_buf_out[channel])) {
		err("%s() insufficient buffer space for %d bytes!", __func__, len);
		return -1;
	}

	len = buf_append(&router->channel_buf_out[channel], data, len);
	ev_io_start(router->loop, &router->w_out);
	return len;
}

const char *mhr_get_serial(struct mh_router *router) {
	return router->serial;
}
