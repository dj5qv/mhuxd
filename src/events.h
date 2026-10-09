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
struct mhc_radio_info;

// Event Types
enum app_event_type {
    EV_KEYER_STATE,       // Keyer lifecycle (ONLINE, OFFLINE, etc.)
    EV_KEYER_MODE,        // Radio mode changed (CW, VOICE, etc.)
    EV_CON_STATUS,        // Connector (TCP/VSP) status changed
    EV_RADIO_INFO,        // Radio info (frequency, mode) from rig mode sync changed or timed out
    EV_CAT_MONITOR,       // One line of the native CAT monitor
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

struct ev_radio_info {
    const char *serial;
    uint8_t radio;                       // 1 or 2
    uint8_t valid;                       // 0: timed out
    const char *source;                  // module that sent it ("rigcl", "catn")
    const struct mhc_radio_info *info;
};

struct ev_cat_monitor {
    const char *serial;
    uint8_t radio;
    const char *line;                    // trace format, see tests/cat/README.md
};

#endif // EVENTS_H