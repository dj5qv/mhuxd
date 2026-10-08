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

typedef struct eventbus eventbus_t;
typedef struct eventbus_sub eventbus_sub_t;

typedef void (*event_cb_fn)(enum app_event_type type, const void *data, void *user_data);

eventbus_t *eventbus_create(void);
void eventbus_destroy(eventbus_t *bus);

eventbus_sub_t *eventbus_subscribe(eventbus_t *bus, enum app_event_type type, event_cb_fn cb, void *user_data);
void eventbus_unsubscribe(eventbus_sub_t *sub);

void eventbus_publish(eventbus_t *bus, enum app_event_type type, const void *data);

#endif // EVENTBUS_H