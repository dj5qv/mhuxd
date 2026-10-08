/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>

struct device;
struct mh_router;

// Event Types
enum app_event_type {
    EV_KEYER_STATE,       // Keyer lifecycle (ONLINE, OFFLINE, etc.)
    EV_KEYER_MODE,        // Radio mode changed (CW, VOICE, etc.)
    EV_CON_STATUS,        // Connector (TCP/VSP) status changed
    EV_MAX                // Keep as last
};

// Event Structs
struct ev_keyer_state {
    const char *serial;
    int state;
};

struct ev_keyer_mode {
    const char *serial;
    uint8_t mode_cur;
    uint8_t mode_r1;
    uint8_t mode_r2;
};

struct ev_con_status {
    int connector_id;
    int status;
};

#endif // EVENTS_H