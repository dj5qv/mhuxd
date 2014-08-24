/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHSM_H
#define MHSM_H

#include <stdint.h>

struct mh_control;

struct sm* sm_create(struct mh_control *ctl);
void sm_destroy (struct sm *sm);
int sm_get_antsw(struct sm *sm);

int sm_get_state_value(const uint8_t mpk_buffer[4], const char *key);
void sm_debug_print_state_values(const uint8_t buffer[9]);

#endif // MHSM_H
