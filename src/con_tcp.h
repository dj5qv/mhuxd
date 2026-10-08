/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CON_TCP_H
#define CON_TCP_H

#include <stdint.h>

struct connector_spec;
struct ctcp *ctcp_create(const struct connector_spec *cpsec, uint8_t is_ptt_channel);
void ctcp_destroy(struct ctcp *ctcp);

#endif // CON_TCP_H
