/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef EVENTBUS_H
#define EVENTBUS_H

#include "events.h"

struct eventbus;
struct eventbus_sub;

typedef void (*event_cb_fn)(enum app_event_type type, const void *data, void *user_data);

struct eventbus *eventbus_create(void);
void eventbus_destroy(struct eventbus *bus);

struct eventbus_sub *eventbus_subscribe(struct eventbus *bus, enum app_event_type type, event_cb_fn cb, void *user_data);
void eventbus_unsubscribe(struct eventbus_sub *sub);

void eventbus_publish(struct eventbus *bus, enum app_event_type type, const void *data);

#endif // EVENTBUS_H