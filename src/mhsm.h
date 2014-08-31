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
struct ev_loop;
struct sm;
struct cfg;

enum {
	SM_OUT_ANT = 0,
	SM_OUT_BPF = 1,
	SM_OUT_SEQ = 2,
	SM_OUT_SEQINV = 3,
	SM_OUT_MAX = 4
};

struct sm* sm_create(struct mh_control *ctl, const char *serial, struct ev_loop *loop);
void sm_destroy (struct sm *sm);
int sm_get_antsw(struct sm *sm);

int sm_antsw_has_bandplan(struct sm *sm);
int sm_antsw_to_cfg(struct sm *sm, struct cfg *cfg);
void sm_antsw_clear_lists(struct sm *sm);
int sm_antsw_set_opt(struct sm *sm, const char *key, uint32_t val);
int sm_antsw_set_output(struct sm *sm, const char *out_name, uint8_t val);

int sm_get_state_value(const uint8_t mpk_buffer[4], const char *key);
void sm_debug_print_state_values(const uint8_t buffer[9]);

#endif // MHSM_H
