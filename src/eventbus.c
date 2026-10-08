/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdlib.h>
#include "eventbus.h"
#include "pglist.h"
#include "util.h"


#define MOD_ID "ebus"

typedef struct eventbus_sub {
    struct PGNode node;
    enum app_event_type type;
    event_cb_fn cb;
    void *user_data;
} eventbus_sub_t;

typedef struct eventbus {
    struct PGList subs[EV_MAX];
} eventbus_t;

eventbus_t *eventbus_create(void) {
    eventbus_t *bus = w_calloc(1, sizeof(*bus));
    for (int i = 0; i < EV_MAX; i++) {
        PG_NewList(&bus->subs[i]);
    }
    return bus;
}

void eventbus_destroy(eventbus_t *bus) {
    if (!bus) return;

    for (int i = 0; i < EV_MAX; i++) {
        eventbus_sub_t *sub;
        while ((sub = (void *)PG_FIRSTENTRY(&bus->subs[i]))) {
            PG_Remove(&sub->node);
            free(sub);
        }
    }
    free(bus);
}

eventbus_sub_t *eventbus_subscribe(eventbus_t *bus, enum app_event_type type, event_cb_fn cb, void *user_data) {
    if (!bus || type >= EV_MAX || !cb) return NULL;

    eventbus_sub_t *sub = w_calloc(1, sizeof(*sub));
    sub->type = type;
    sub->cb = cb;
    sub->user_data = user_data;

    PG_AddTail(&bus->subs[type], &sub->node);
    return sub;
}

void eventbus_unsubscribe(eventbus_sub_t *sub) {
    if (!sub) return;
    PG_Remove(&sub->node);
    free(sub);
}

void eventbus_publish(eventbus_t *bus, enum app_event_type type, const void *data) {
    if (!bus || type >= EV_MAX) return;

    eventbus_sub_t *sub;
    PG_SCANLIST(&bus->subs[type], sub) {
        sub->cb(type, data, sub->user_data);
    }
}