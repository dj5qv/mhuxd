/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2017  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHROUTER_H
#define MHROUTER_H

struct mh_router;
struct buffer;

enum {
	MHROUTER_CONNECTED = 1,
	MHROUTER_DISCONNECTED = 2,
};

typedef void (*MHRConsumerCallback)(struct mh_router *, unsigned const char *data ,int len, int channel, void *user_data);
typedef void (*MHRStatusCallback)(struct mh_router *, int status, void *user_data);
typedef void (*MHRProcessorCallback)(struct mh_router *, struct buffer *, void *user_data);


// core functions
struct mh_router *mhr_create(struct ev_loop *loop, const char *serial, uint8_t has_flags_channel);
void mhr_destroy(struct mh_router *router);
void mhr_set_keyer_fd(struct mh_router *, int fd);
const char *mhr_get_serial(struct mh_router *);
int mhr_send_in(struct mh_router *router, const uint8_t *data, unsigned int len, int channel);
void mhr_set_bps_limit(struct mh_router *router, int channel, float bps);


// consumer / producer interface
void mhr_add_consumer(struct mh_router *router, int fd, int channel);
void mhr_add_producer(struct mh_router *router, int fd, int channel);
void mhr_add_consumer_cb(struct mh_router *router, MHRConsumerCallback, int channel, void *user_data);
void mhr_rem_consumer(struct mh_router *router, int fd, int channel);
void mhr_rem_producer(struct mh_router *router, int fd, int channel);
void mhr_rem_consumer_cb(struct mh_router *router, MHRConsumerCallback, int channel);

// processor interface (deprecated)
void mhr_add_processor_cb(struct mh_router *router, MHRProcessorCallback callback, int channel, void *user_data);
void mhr_rem_processor_cb(struct mh_router *router, MHRProcessorCallback callback, int channel);
void mhr_send_out(struct mh_router *router, const uint8_t *data, unsigned int len, int channel);

// generic callbacks
void mhr_add_status_cb(struct mh_router *router, MHRStatusCallback callback, void *user_data);
void mhr_rem_status_cb(struct mh_router *router, MHRStatusCallback callback);


#endif // MHROUTER_H
