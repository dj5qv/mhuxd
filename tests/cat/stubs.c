/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stubs.h"
#include "app_ctx.h"
#include "events.h"
#include "logger.h"
#include "util.h"

struct ev_loop *loop;
int failures;

int log_level = 0;
void log_msg(int severity, const char *header, const char *fmt, ...) {
	va_list ap;
	(void)severity;
	if(!getenv("VERBOSE"))
		return;
	printf("    %s ", header);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void log_hex(int severity, const char *msg1, const char *msg2, const char *msg3, const char *buf, int len) {
	(void)severity; (void)msg1; (void)msg2; (void)msg3; (void)buf; (void)len;
}

void *w_malloc(size_t size) {
	void *p = malloc(size);
	if(!p)
		abort();
	return p;
}

void *w_calloc(size_t nmemb, size_t size) {
	void *p = calloc(nmemb, size);
	if(!p)
		abort();
	return p;
}

char *w_strdup(const char *src) {
	char *p = strdup(src);
	if(!p)
		abort();
	return p;
}

int fd_close(int *fd) {
	if(!fd || *fd < 0)
		return 0;
	close(*fd);
	*fd = -1;
	return 1;
}

struct ev_loop *app_ctx_get_loop(app_ctx *ctx) {
	(void)ctx;
	return loop;
}

eventbus_t *app_ctx_get_eventbus(app_ctx *ctx) {
	(void)ctx;
	return NULL;
}

event_cb_fn keyer_state_cb;
static void *keyer_state_data;
int mon_lines;
char mon_last[256];

void eventbus_publish(eventbus_t *bus, enum app_event_type type, const void *data) {
	(void)bus;
	if(type == EV_CAT_MONITOR) {
		const struct ev_cat_monitor *ev = data;
		mon_lines++;
		snprintf(mon_last, sizeof(mon_last), "%s", ev->line);
	}
}

eventbus_sub_t *eventbus_subscribe(eventbus_t *bus, enum app_event_type type, event_cb_fn cb, void *user_data) {
	(void)bus;
	if(type == EV_KEYER_STATE) {
		keyer_state_cb = cb;
		keyer_state_data = user_data;
	}
	return (eventbus_sub_t *)&keyer_state_cb;
}

void eventbus_unsubscribe(eventbus_sub_t *sub) {
	(void)sub;
	keyer_state_cb = NULL;
}

void keyer_state(int state) {
	struct ev_keyer_state ev = { .serial = "TEST0001", .state = state };
	if(keyer_state_cb)
		keyer_state_cb(EV_KEYER_STATE, &ev, keyer_state_data);
}

uint8_t keyer_state_val = MHC_KEYER_STATE_ONLINE;

uint8_t mhc_get_state(struct mh_control *ctl) {
	(void)ctl;
	return keyer_state_val;
}

const char *mhc_state_str(int state) {
	return state == MHC_KEYER_STATE_ONLINE ? "ONLINE" : "OFFLINE";
}

struct mhc_speed_cfg speed;
int speed_valid;

int mhc_get_speed_params(struct mh_control *ctl, int channel, struct mhc_speed_cfg *out) {
	(void)ctl; (void)channel;
	if(!speed_valid)
		return -1;
	*out = speed;
	return 0;
}

struct mhc_radio_info pub[MAX_PUB];
int npub;

void mhc_update_radio_info(struct mh_control *ctl, const char *source, const struct mhc_radio_info *info) {
	(void)ctl; (void)source;
	if(npub < MAX_PUB)
		pub[npub++] = *info;
}

const struct mhc_radio_info *last_pub(void) {
	return npub ? &pub[npub - 1] : NULL;
}

static void stop_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
	(void)w; (void)revents;
	ev_break(l, EVBREAK_ONE);
}

void run_for(double s) {
	ev_timer t;
	ev_timer_init(&t, stop_cb, s, 0.);
	ev_timer_start(loop, &t);
	ev_run(loop, 0);
	ev_timer_stop(loop, &t);
}

size_t parse_hex(const char *s, uint8_t *buf) {
	size_t n = 0;
	unsigned v;
	int off;
	while(sscanf(s, " %2x%n", &v, &off) == 1) {
		buf[n++] = v;
		s += off;
	}
	return n;
}
