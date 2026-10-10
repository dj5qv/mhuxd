/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2016  Matthias Moeller, DJ5QV
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
// fd: the producer the data comes from.
typedef void (*MHRTapCallback)(struct mh_router *, unsigned const char *data, int len, int channel, int fd, void *user_data);
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
void mhr_add_consumer(struct mh_router *router, int fd, int channel, const char *owner_tag);
void mhr_add_producer(struct mh_router *router, int fd, int channel, const char *owner_tag);
void mhr_add_endpoint_fd(struct mh_router *router, int fd, int channel, const char *owner_tag);
void mhr_add_consumer_cb(struct mh_router *router, MHRConsumerCallback, int channel, void *user_data);
void mhr_rem_consumer(struct mh_router *router, int fd, int channel);
void mhr_rem_producer(struct mh_router *router, int fd, int channel);
void mhr_rem_endpoint_fd(struct mh_router *router, int fd, int channel);
void mhr_rem_consumer_cb(struct mh_router *router, MHRConsumerCallback, int channel);

// Read-only tap: called with the data that producers (connectors) send to the keyer on a
// channel, as it is read. Removal matches callback and user_data.
void mhr_add_producer_tap_cb(struct mh_router *router, MHRTapCallback, int channel, void *user_data);
void mhr_rem_producer_tap_cb(struct mh_router *router, MHRTapCallback, int channel, void *user_data);

// Native CAT (src/cat_native.c). Connectors are identified by their endpoint fd, which is
// producer and consumer at once.
//
// While held, the router doesn't read the data the connectors send on the channel; it waits
// in their socket buffers.
void mhr_hold_producers(struct mh_router *router, int channel, int hold);
// Only the producer fd has the turn: the others' data waits in their socket buffers. -1: all
// producers may send. The turn stays with fd when that producer is removed, the caller passes
// it on (the answers to its last commands may still be on their way).
void mhr_set_producer_turn(struct mh_router *router, int channel, int fd);
// The next producer of the channel after fd, round robin, with data waiting in its socket;
// -1 if none.
int mhr_next_waiting_producer(struct mh_router *router, int channel, int fd);
int mhr_count_producers(struct mh_router *router, int channel);
// owner_tag of the producer, NULL if fd is no producer of the channel.
const char *mhr_get_producer_tag(struct mh_router *router, int channel, int fd);
// While intercepted, data from the keyer on the channel goes to the consumer callbacks only;
// one of them passes on what the connectors should get with mhr_pass_out(), to the consumer
// fd or to all (MHR_ALL).
#define MHR_ALL (-1)
void mhr_intercept_consumers(struct mh_router *router, int channel, int intercept);
void mhr_pass_out(struct mh_router *router, const uint8_t *data, unsigned int len, int channel, int fd);

// processor interface (deprecated)
void mhr_add_processor_cb(struct mh_router *router, MHRProcessorCallback callback, int channel, void *user_data);
void mhr_rem_processor_cb(struct mh_router *router, MHRProcessorCallback callback, int channel);
void mhr_send_out(struct mh_router *router, const uint8_t *data, unsigned int len, int channel);

// generic callbacks
void mhr_add_status_cb(struct mh_router *router, MHRStatusCallback callback, void *user_data);
void mhr_rem_status_cb(struct mh_router *router, MHRStatusCallback callback);

// Some state for Winkey PTT routing.
void mhr_set_wk_tx_focus(struct mh_router *router, uint8_t focus);

#endif // MHROUTER_H
