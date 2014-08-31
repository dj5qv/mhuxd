/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include <ev.h>
#include "mhsm.h"
#include "pglist.h"
#include "util.h"
#include "buffer.h"
#include "logger.h"
#include "citem.h"
#include "mhcontrol.h"
#include "cfgnod.h"

#ifdef SMSIM
#include "mhsm_sim.h"
#endif

#define MAX_NAME_LEN (10)
#define MAX_LABEL_LEN (5)

#define ANTFLAG_STEPPIR (1<<0)
#define ANTFLAG_RX_ONLY (1<<1)
#define GRPFLAG_TYPE (1<<0)

enum {
	STATE_GET_ANTSW_EMPTY = 0,
	STATE_GET_ANTSW_FIXED = 1,
	STATE_GET_ANTSW_ANTREC_LENTGH = 2,
	STATE_GET_ANTSW_ANTREC = 3,
	STATE_GET_ANTSW_GRPREC_LENTGH = 4,
	STATE_GET_ANTSW_GRPREC = 5,
	STATE_GET_ANTSW_BREC_LENTGH = 6,
	STATE_GET_ANTSW_BREC = 7,
	STATE_GET_ANTSW_DONE = 8
};

struct antenna_record {
	struct PGNode node;
	struct buffer raw_buf;
	char label[MAX_LABEL_LEN + 1];
	char name[MAX_NAME_LEN + 1];
	uint32_t output_settings;
	uint8_t steppir : 1;
	uint8_t rxonly : 1;
	uint8_t pa_ant_number;
	uint8_t rotator;
	uint16_t rotator_offset;
};

struct reference {
	struct PGNode node;
	uint8_t idx;
	uint8_t flags;
	uint16_t min_azimuth;
	uint16_t max_azimuth;
};

struct groups_record {
	struct PGNode node;
	struct buffer raw_buf;
	char label[MAX_LABEL_LEN + 1];
	char name[MAX_NAME_LEN + 1];
	uint8_t	num_antennas;
	struct PGList ant_ref_list;
	uint16_t min_azimuth;
	uint16_t max_azimuth;
	uint8_t flags;
	uint16_t current_azimuth;
};

struct band_record {
	struct PGNode node;
	struct buffer raw_buf;
	uint32_t low_freq;
	uint32_t high_freq;
	char name[MAX_NAME_LEN + 1];
	uint8_t outputs;
	uint32_t bpf_sequencer;
	struct PGList ant_ref_list;
	struct PGList grp_ref_list;
	uint8_t currentRx;
	uint8_t currentTx;
	uint8_t split;
};

struct sm_bandplan {
	uint8_t fixed_data[115];
	struct PGList antenna_list;
	struct PGList groups_list;
	struct PGList band_list;

};

struct sm {
	const char *serial;
	struct ev_loop *loop;
	struct mh_control *ctl;
	struct sm_bandplan *bp_eeprom;
	struct sm_bandplan *bp_cfg;
	uint8_t get_antsw_state;
	uint16_t get_antsw_offset;
	uint8_t get_antsw_to_read;

};

static void debug_print_antsw_values(struct sm_bandplan *bp);

struct output_map {
	const char *label;
	uint8_t bit;
};

struct output_map output_map[] = {
{ "B6", 1 },
{ "A10", 2},
{ "B5", 3 },
{ "A9", 4 },
{ "A8", 8 },
{ "B4", 9 },
{ "A7", 10 },
{ "B2", 11 },
{ "A6", 12 },
{ "B1", 13 },
{ "A5", 14 },
{ "B10", 15 },
{ "A4", 16 },
{ "B9", 17 },
{ "A3", 18 },
{ "B8", 19 },
{ "A2", 20 },
{ "B7", 21 },
{ "A1", 22 },
{ "B3", 23 }
};

struct citem sm_state_items[] = {
	CITEM("curAzimuth", 0, 15, 16),
	CITEM("azimuthOffset", 2, 15, 16),
	CITEM("targetAzimuth", 4, 15, 16),
	CITEM("portAVolt", 6, 7, 8 ),
	CITEM("portBVolt", 7, 7, 8 ),
	CITEM("rotatorSelected", 8, 0, 1 ),
	CITEM("virtualRotatorSelected", 8, 1, 1 ),
	CITEM("steppirVer", 9, 7, 8),
	CITEM("currentVrAzimuth", 10, 15, 16),
	CITEM("steppirVerHi", 12, 7, 8)
};

struct citem sm_bandplan_fixed_items[] = {
	CITEM("formatVersion", 0, 7, 8),
#if 0
	CITEM("sequencerOutputs.B6", 1, 1, 1),
	CITEM("sequencerOutputs.A10", 1, 2, 1),
	CITEM("sequencerOutputs.B5", 1, 3, 1),
	CITEM("sequencerOutputs.A9", 1, 4, 1),
	CITEM("sequencerOutputs.A8", 1, 8, 1),
	CITEM("sequencerOutputs.B4", 1, 9, 1),
	CITEM("sequencerOutputs.A7", 1, 10, 1),
	CITEM("sequencerOutputs.B2", 1, 11, 1),
	CITEM("sequencerOutputs.A6", 1, 12, 1),
	CITEM("sequencerOutputs.B1", 1, 13, 1),
	CITEM("sequencerOutputs.A5", 1, 14, 1),
	CITEM("sequencerOutputs.B10", 1, 15, 1),
	CITEM("sequencerOutputs.A4", 1, 16, 1),
	CITEM("sequencerOutputs.B9", 1, 17, 1),
	CITEM("sequencerOutputs.A3", 1, 18, 1),
	CITEM("sequencerOutputs.B8", 1, 19, 1),
	CITEM("sequencerOutputs.A2", 1, 20, 1),
	CITEM("sequencerOutputs.B7", 1, 21, 1),
	CITEM("sequencerOutputs.A1", 1, 22, 1),
	CITEM("sequencerOutputs.B3", 1, 23, 1),

	CITEM("bandDependent.B6", 4, 1, 1),
	CITEM("bandDependent.A10", 4, 2, 1),
	CITEM("bandDependent.B5", 4, 3, 1),
	CITEM("bandDependent.A9", 4, 4, 1),
	CITEM("bandDependent.A8", 4, 8, 1),
	CITEM("bandDependent.B4", 4, 9, 1),
	CITEM("bandDependent.A7", 4, 10, 1),
	CITEM("bandDependent.B2", 4, 11, 1),
	CITEM("bandDependent.A6", 4, 12, 1),
	CITEM("bandDependent.B1", 4, 13, 1),
	CITEM("bandDependent.A5", 4, 14, 1),
	CITEM("bandDependent.B10", 4, 15, 1),
	CITEM("bandDependent.A4", 4, 16, 1),
	CITEM("bandDependent.B9", 4, 17, 1),
	CITEM("bandDependent.A3", 4, 18, 1),
	CITEM("bandDependent.B8", 4, 19, 1),
	CITEM("bandDependent.A2", 4, 20, 1),
	CITEM("bandDependent.B7", 4, 21, 1),
	CITEM("bandDependent.A1", 4, 22, 1),
	CITEM("bandDependent.B3", 4, 23, 1),
#else
	CITEM("sequencerOutputs", 1, 23, 24),
	CITEM("bandDependent", 4, 23, 24),
#endif
	CITEM("civFunc", 7, 7, 8),
	CITEM("civBaudRate", 8, 7, 8),
	CITEM("civAddress", 9, 7, 8),
	CITEM("extSerFunc", 10, 7, 8),
	CITEM("extSerBaudRate", 11, 7, 8),
	CITEM("extSerPar", 12, 7, 8),   // not implemented in SM 1.3 ?
	CITEM("antSwDelay", 13, 15, 16),
	CITEM("bbmDelay", 15, 7, 8),
	CITEM("inhibitLead", 16, 15, 16),
	CITEM("useKeyIn", 18, 0, 1),
	CITEM("invertKeyIn", 18, 1, 1),

	/*
	CITEM("sequencer.lead.", 19,0, 16),
	CITEM("sequencer.tail.", 21,0, 16),
	*/
	CITEM("sequencer.lead.B6",  23, 15, 16),
	CITEM("sequencer.tail.B6",  25, 15, 16),
	CITEM("sequencer.lead.A10", 27, 15, 16),
	CITEM("sequencer.tail.A10", 29, 15, 16),
	CITEM("sequencer.lead.B5",  31, 15, 16),
	CITEM("sequencer.tail.B5",  33, 15, 16),
	CITEM("sequencer.lead.A9",  35, 15, 16),
	CITEM("sequencer.tail.A9",  37, 15, 16),
	/*
	CITEM("sequencer.lead.",    39, 15, 16),
	CITEM("sequencer.tail.",    41, 15, 16),
	CITEM("sequencer.lead.",    43, 15, 16),
	CITEM("sequencer.tail.",    45, 15, 16),
	CITEM("sequencer.lead.",    47, 15, 16),
	CITEM("sequencer.tail.",    49, 15, 16),
	*/
	CITEM("sequencer.lead.A8",  51, 15, 16),
	CITEM("sequencer.tail.A8",  53, 15, 16),
	CITEM("sequencer.lead.B4",  55, 15, 16),
	CITEM("sequencer.tail.B4",  57, 15, 16),
	CITEM("sequencer.lead.A7",  59, 15, 16),
	CITEM("sequencer.tail.A7",  61, 15, 16),
	CITEM("sequencer.lead.B2",  63, 15, 16),
	CITEM("sequencer.tail.B2",  65, 15, 16),
	CITEM("sequencer.lead.A6",  67, 15, 16),
	CITEM("sequencer.tail.A6",  69, 15, 16),
	CITEM("sequencer.lead.B1",  71, 15, 16),
	CITEM("sequencer.tail.B1",  73, 15, 16),
	CITEM("sequencer.lead.A5",  75, 15, 16),
	CITEM("sequencer.tail.A5",  77, 15, 16),
	CITEM("sequencer.lead.B10", 79, 15, 16),
	CITEM("sequencer.tail.B10", 81, 15, 16),
	CITEM("sequencer.lead.A4",  83, 15, 16),
	CITEM("sequencer.tail.A4",  85, 15, 16),
	CITEM("sequencer.lead.B9",  87, 15, 16),
	CITEM("sequencer.tail.B9",  89, 15, 16),
	CITEM("sequencer.lead.A3",  91, 15, 16),
	CITEM("sequencer.tail.A3",  93, 15, 16),
	CITEM("sequencer.lead.B8",  95, 15, 16),
	CITEM("sequencer.tail.B8",  97, 15, 16),
	CITEM("sequencer.tail.A2",  99, 15, 16),
	CITEM("sequencer.tail.A2", 101, 15, 16),
	CITEM("sequencer.tail.B7", 103, 15, 16),
	CITEM("sequencer.tail.B7", 105, 15, 16),
	CITEM("sequencer.tail.A1", 107, 15, 16),
	CITEM("sequencer.tail.A1", 109, 15, 16),
	CITEM("sequencer.tail.B3", 111, 15, 16),
	CITEM("sequencer.tail.B3", 113, 15, 16),
};

static struct sm_bandplan *bp_create() {
	struct sm_bandplan *bp = w_calloc(1, sizeof(*bp));
	PG_NewList(&bp->antenna_list);
	PG_NewList(&bp->groups_list);
	PG_NewList(&bp->band_list);
	return bp;
}

static void clear_lists(struct sm_bandplan *bp) {
	struct antenna_record *arec;
	while((arec = (void*)PG_FIRSTENTRY(&bp->antenna_list))) {
		PG_Remove(&arec->node);
		free(arec);
	}

	struct groups_record *grec;
	while((grec = (void*)PG_FIRSTENTRY(&bp->groups_list))) {
		struct reference *ref;
		while(( ref = (void*)PG_FIRSTENTRY(&grec->ant_ref_list))) {
			PG_Remove(&ref->node);
			free(ref);
		}
		PG_Remove(&grec->node);
		free(grec);
	}

	struct band_record *brec;
	while((brec = (void*)PG_FIRSTENTRY(&bp->band_list))) {
		struct reference *ref;
		while(( ref = (void*)PG_FIRSTENTRY(&brec->ant_ref_list))) {
			PG_Remove(&ref->node);
			free(ref);
		}
		while(( ref = (void*)PG_FIRSTENTRY(&brec->grp_ref_list))) {
			PG_Remove(&ref->node);
			free(ref);
		}
		PG_Remove(&brec->node);
		free(brec);
	}
}

static void bp_destroy(struct sm_bandplan *bp) {
	clear_lists(bp);
	free(bp);
}


#define BGETC(buf) \
	c = buf_get_c(buf); \
	if(c < 0) \
		return -1;

static int decode_arec(struct antenna_record *arec) {
	int i, c, len;

	dbg1_h("(sm) arec raw: ", arec->raw_buf.data, arec->raw_buf.size);
	BGETC(&arec->raw_buf); // label len
	len = c;
	if(len > MAX_LABEL_LEN)
		len = MAX_LABEL_LEN;
	for(i = 0; i < len; i++) {
		BGETC(&arec->raw_buf);
		arec->label[i] = c;
	}

	BGETC(&arec->raw_buf); // name len
	len = c;
	if(len > MAX_NAME_LEN)
		len = MAX_NAME_LEN;
	for(i = 0; i < len; i++) {
		BGETC(&arec->raw_buf);
		arec->name[i] = c;
	}

	BGETC(&arec->raw_buf);
	arec->output_settings = c;
	BGETC(&arec->raw_buf);
	arec->output_settings |= (c << 8);
	BGETC(&arec->raw_buf);
	arec->output_settings |= (c << 16);

	BGETC(&arec->raw_buf);
	arec->steppir = c & 1;
	arec->rxonly = (c >> 1) & 1;

	BGETC(&arec->raw_buf);
	arec->pa_ant_number = c;

	BGETC(&arec->raw_buf);
	arec->rotator = c;

	if(c == 1) {
		BGETC(&arec->raw_buf);
		arec->rotator_offset = c;
		BGETC(&arec->raw_buf);
		arec->rotator_offset |= (c << 8);
	}

	return 0;
}

static int decode_grec(struct groups_record *grec) {
	int i, c, len;

	dbg1_h("(sm) grec raw: ", grec->raw_buf.data, grec->raw_buf.size);
	BGETC(&grec->raw_buf); // label len
	len = c;
	if(len > MAX_LABEL_LEN)
		len = MAX_LABEL_LEN;
	for(i = 0; i < len; i++) {
		BGETC(&grec->raw_buf);
		grec->label[i] = c;
	}

	BGETC(&grec->raw_buf); // name len
	len = c;
	if(len > MAX_NAME_LEN)
		len = MAX_NAME_LEN;
	for(i = 0; i < len; i++) {
		BGETC(&grec->raw_buf);
		grec->name[i] = c;
	}

	BGETC(&grec->raw_buf); // number of antennas
	grec->num_antennas = c;
	for(i = 0; i < grec->num_antennas; i++) {
		struct reference *ref = w_calloc(1, sizeof(*ref));
		BGETC(&grec->raw_buf);
		ref->idx = c;
		BGETC(&grec->raw_buf);
		ref->min_azimuth = c;
		BGETC(&grec->raw_buf);
		ref->min_azimuth |= (c << 8);
		BGETC(&grec->raw_buf);
		ref->max_azimuth = c;
		BGETC(&grec->raw_buf);
		ref->max_azimuth |= (c << 8);
		PG_AddTail(&grec->ant_ref_list, &ref->node);
	}

	BGETC(&grec->raw_buf);
	grec->flags = c;

	BGETC(&grec->raw_buf);
	grec->current_azimuth = c;
	BGETC(&grec->raw_buf);
	grec->current_azimuth |= (c << 8);

	return 0;
}

static int decode_brec(struct band_record *brec) {
	int i, c, len, antcnt;

	dbg1_h("(sm) raw: ", brec->raw_buf.data, brec->raw_buf.size);
	BGETC(&brec->raw_buf);
	brec->low_freq = c;
	BGETC(&brec->raw_buf);
	brec->low_freq |= (c << 8);
	BGETC(&brec->raw_buf);
	brec->low_freq |= (c << 16);
	BGETC(&brec->raw_buf);
	brec->low_freq |= (c << 24);

	BGETC(&brec->raw_buf);
	brec->high_freq = c;
	BGETC(&brec->raw_buf);
	brec->high_freq |= (c << 8);
	BGETC(&brec->raw_buf);
	brec->high_freq |= (c << 16);
	BGETC(&brec->raw_buf);
	brec->high_freq |= (c << 24);

	BGETC(&brec->raw_buf); // name len
	len = c;
	if(len > MAX_NAME_LEN)
		len = MAX_NAME_LEN;
	for(i = 0; i < len; i++) {
		BGETC(&brec->raw_buf);
		brec->name[i] = c;
	}

	BGETC(&brec->raw_buf);
	brec->outputs = c;

	BGETC(&brec->raw_buf);
	brec->bpf_sequencer = c;
	BGETC(&brec->raw_buf);
	brec->bpf_sequencer |= (c << 8);
	BGETC(&brec->raw_buf);
	brec->bpf_sequencer |= (c << 16);

	BGETC(&brec->raw_buf);
	antcnt = c;
	for(i = 0; i < antcnt; i++) {
		int ba0;
		BGETC(&brec->raw_buf);
		ba0 = c;
		struct reference *ref = w_calloc(1, sizeof(*ref));
		if(ba0 > 0) {
			// linked to group
			BGETC(&brec->raw_buf); // ignore ba1
			BGETC(&brec->raw_buf); // flags;
			ref->idx = ba0 - 1;
			ref->flags = c;
			PG_AddTail(&brec->grp_ref_list, &ref->node);
		} else {
			// linked to antenna
			BGETC(&brec->raw_buf); // ba1
			ref->idx = c;
			BGETC(&brec->raw_buf); // ba2, flags
			ref->flags = c;
			PG_AddTail(&brec->ant_ref_list, &ref->node);
		}
	}

	BGETC(&brec->raw_buf);
	brec->currentRx = c;
	BGETC(&brec->raw_buf);
	brec->currentTx = c;
	BGETC(&brec->raw_buf);
	brec->split = c;

	return 0;
}

struct sm* sm_create(struct mh_control *ctl, const char *serial, struct ev_loop *loop) {
	struct sm *sm = w_calloc(1, sizeof(*sm));
	sm->serial = serial;
	sm->loop = loop;
	sm->ctl = ctl;
	sm->get_antsw_state = STATE_GET_ANTSW_EMPTY;
#ifdef SMSIM
	smsim_init();
#endif
	return sm;
}

void sm_destroy (struct sm *sm) {
	if(sm->bp_eeprom)
		bp_destroy(sm->bp_eeprom);
	if(sm->bp_cfg)
		bp_destroy(sm->bp_cfg);
	free(sm);
}

static void get_antsw_completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data)  {
	(void)reply_buf; (void)len;
	struct sm *sm = user_data;
	int i;

	if(result != CMD_RESULT_OK) {
		err("(sm) %s command failed: %s!", "GET ANTSW BLOCK", mhc_cmd_err_string(result));
		sm->get_antsw_state = STATE_GET_ANTSW_EMPTY;
		return;
	}

	if(len != 34) {
		err("(sm) GET ANTSW BLOCK invalid response length: %d!", len);
		sm->get_antsw_state = STATE_GET_ANTSW_EMPTY;
		return;
	}

	reply_buf++;
	len -= 2;

	dbg1("(sm) GET ANTSW BLOCK offset %d ok, state %d", sm->get_antsw_offset, sm->get_antsw_state);
	dbg1_h("(sm) GET ANTSW BLOCK data:", reply_buf, len);

	for(i = 0; i < 32; i++) {
		uint8_t c = reply_buf[i];
		struct antenna_record *arec;
		struct groups_record *grec;
		struct band_record *brec;

		switch(sm->get_antsw_state) {
		case STATE_GET_ANTSW_EMPTY:
			break;

		case STATE_GET_ANTSW_FIXED:
			sm->bp_eeprom->fixed_data[sm->get_antsw_offset + i] = c;
			sm->get_antsw_to_read--;
			if(sm->get_antsw_to_read == 0)
				sm->get_antsw_state = STATE_GET_ANTSW_ANTREC_LENTGH;
			break;

		case STATE_GET_ANTSW_ANTREC_LENTGH:
			if(c > 0) {
				sm->get_antsw_state = STATE_GET_ANTSW_ANTREC;
				sm->get_antsw_to_read = c;
				arec = w_calloc(1, sizeof(*arec));
				PG_AddTail(&sm->bp_eeprom->antenna_list, &arec->node);
			} else {
				sm->get_antsw_state = STATE_GET_ANTSW_GRPREC_LENTGH;
			}
			break;

		case STATE_GET_ANTSW_ANTREC:
			arec = (void*)PG_LASTENTRY(&sm->bp_eeprom->antenna_list);
			buf_append_c(&arec->raw_buf, c);
			sm->get_antsw_to_read--;
			if(sm->get_antsw_to_read == 0) {
				sm->get_antsw_state = STATE_GET_ANTSW_ANTREC_LENTGH;
			}
			break;

		case STATE_GET_ANTSW_GRPREC_LENTGH:
			if(c > 0) {
				sm->get_antsw_state = STATE_GET_ANTSW_GRPREC;
				sm->get_antsw_to_read = c;
				grec = w_calloc(1, sizeof(*grec));
				PG_NewList(&grec->ant_ref_list);
				PG_AddTail(&sm->bp_eeprom->groups_list, &grec->node);
			} else {
				sm->get_antsw_state = STATE_GET_ANTSW_BREC_LENTGH;
			}
			break;

		case STATE_GET_ANTSW_GRPREC:
			grec = (void*)PG_LASTENTRY(&sm->bp_eeprom->groups_list);
			buf_append_c(&grec->raw_buf, c);
			sm->get_antsw_to_read--;
			if(sm->get_antsw_to_read == 0) {
				sm->get_antsw_state = STATE_GET_ANTSW_GRPREC_LENTGH;
			}
			break;

		case STATE_GET_ANTSW_BREC_LENTGH:
			if(c > 0) {
				sm->get_antsw_state = STATE_GET_ANTSW_BREC;
				sm->get_antsw_to_read = c;
				brec = w_calloc(1, sizeof(*brec));
				PG_NewList(&brec->ant_ref_list);
				PG_NewList(&brec->grp_ref_list);
				PG_AddTail(&sm->bp_eeprom->band_list, &brec->node);
			} else {
				sm->get_antsw_state = STATE_GET_ANTSW_DONE;
			}
			break;

		case STATE_GET_ANTSW_BREC:
			brec = (void*)PG_LASTENTRY(&sm->bp_eeprom->band_list);
			buf_append_c(&brec->raw_buf, c);
			sm->get_antsw_to_read--;
			if(sm->get_antsw_to_read == 0) {
				sm->get_antsw_state = STATE_GET_ANTSW_BREC_LENTGH;
			}
			break;

		case STATE_GET_ANTSW_DONE:
			break;

		}
	}

	if(sm->get_antsw_state != STATE_GET_ANTSW_DONE) {
		// not finished yet
		sm->get_antsw_offset += 32;
#ifdef SMSIM
		{
			struct buffer b;
			buf_reset(&b);
			buf_append_c(&b, 0x43); // GET ANTSW BLOCK
			buf_append(&b, bp_buf.data + sm->get_antsw_offset, 32);
			buf_append_c(&b, 0x43 | (1<<7)); // GET ANTSW BLOCK
			get_antsw_completion_cb(b.data, b.size, CMD_RESULT_OK, sm);
		}

#else
		if(-1 == mhc_sm_get_antsw_block(sm->ctl, sm->get_antsw_offset, get_antsw_completion_cb, sm)) {
			sm->get_antsw_state = STATE_GET_ANTSW_EMPTY;
			return;
		}
#endif
	} else {
		struct antenna_record *arec;
		struct groups_record *grec;
		struct band_record *brec;
		int i = 0;

		citem_debug_print_values("sm antsw fixed", sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), sm->bp_eeprom->fixed_data, 115);

		PG_SCANLIST(&sm->bp_eeprom->antenna_list, arec) {
			dbg1("(sm) decode arec %d", i++);
			if(-1 == decode_arec(arec))
				err("(sm) error decoding antenna record!");
		}
		i = 0;
		PG_SCANLIST(&sm->bp_eeprom->groups_list, grec) {
			dbg1("(sm) decode grec %d", i++);
			if(-1 == decode_grec(grec))
				err("(sm) error decoding groups record!");
		}
		i = 0;
		PG_SCANLIST(&sm->bp_eeprom->band_list, brec) {
			dbg1("(sm) decode brec %d", i++);
			if(-1 == decode_brec(brec))
				err("(sm) error decoding band record!");
		}

		debug_print_antsw_values(sm->bp_eeprom);
	}
}

int sm_get_antsw(struct sm *sm) {

	dbg1("(sm) %s", __func__);

	if(sm->get_antsw_state != STATE_GET_ANTSW_EMPTY &&
			sm->get_antsw_state != STATE_GET_ANTSW_DONE) {
		err("(sm) %s failed, command pending!", __func__);
		return -1;
	}

#if 0
	if(! mhc_is_online(sm->ctl))
		return -1;
#endif
	if(sm->bp_eeprom)
		bp_destroy(sm->bp_eeprom);

	sm->bp_eeprom = bp_create();

	sm->get_antsw_offset = 0;
	sm->get_antsw_state = STATE_GET_ANTSW_FIXED;
	sm->get_antsw_to_read = 115;

#ifdef SMSIM
	{
		struct buffer b;
		buf_reset(&b);
		buf_append_c(&b, 0x43); // GET ANTSW BLOCK
		buf_append(&b, bp_buf.data + sm->get_antsw_offset, 32);
		buf_append_c(&b, 0x43 | (1<<7)); // GET ANTSW BLOCK
		get_antsw_completion_cb(b.data, b.size, CMD_RESULT_OK, sm);
	}
#else
	if(-1 == mhc_sm_get_antsw_block(sm->ctl, sm->get_antsw_offset, get_antsw_completion_cb, sm)) {
		sm->get_antsw_state = STATE_GET_ANTSW_EMPTY;
		return -1;
	}
#endif

	while(sm->get_antsw_state != STATE_GET_ANTSW_EMPTY && sm->get_antsw_state != STATE_GET_ANTSW_DONE) {
		ev_loop(sm->loop, EVRUN_ONCE);
	}

	return sm->get_antsw_state == STATE_GET_ANTSW_DONE ? 0 : -1;
}

int sm_get_state_value(const uint8_t buffer[13], const char *key) {
	return citem_get_value(sm_state_items, ARRAY_SIZE(sm_state_items), buffer, 13, key);
}

void sm_debug_print_state_values(const uint8_t buffer[13]) {
	citem_debug_print_values("sm state", sm_state_items, ARRAY_SIZE(sm_state_items), buffer, 13);
}

static void debug_print_antsw_values(struct sm_bandplan *bp) {
	struct antenna_record *arec;
	struct groups_record *grec;
	struct band_record *brec;
	struct reference *ref;

	int i, j;
	dbg1("(sm) %s", __func__);
	citem_debug_print_values("sm antsw fixed area", sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data, 115);

	dbg1("(sm) antenna list");
	i = 0;
	PG_SCANLIST(&bp->antenna_list, arec) {
		dbg1("(sm)  ant index %d", i++);
		dbg1("(sm)    label:       %s", arec->label);
		dbg1("(sm)    name:        %s", arec->name);
		dbg1("(sm)    output:      0x%08x", arec->output_settings);
		dbg1("(sm)    steppir:     %d", arec->steppir);
		dbg1("(sm)    rxonly:      %d", arec->rxonly);
		dbg1("(sm)    paAntNumber: %d", arec->pa_ant_number);
		dbg1("(sm)    rotator:     %d", arec->rotator);
		dbg1("(sm)    rotator off: %d", arec->rotator_offset);
	}

	dbg1("(sm) groups list");
	i = 0;
	PG_SCANLIST(&bp->groups_list, grec) {
		dbg1("(sm)  groups index %d", i++);
		dbg1("(sm)    label:        %s", grec->label);
		dbg1("(sm)    name:         %s", grec->name);
		dbg1("(sm)    num antennas: %d", grec->num_antennas);
		dbg1("(sm)    flags:        %d (%s)", grec->flags, grec->flags & 1 ? "antenna group" : "virtual rotator");
		dbg1("(sm)    current az    %d", grec->current_azimuth);
		j = 0;
		PG_SCANLIST(&grec->ant_ref_list, ref)  {
			dbg1("(sm)    ant list element %d", j++);
			dbg1("(sm)     ant index: %d", ref->idx);
			dbg1("(sm)     ant min azimuth: %d", ref->min_azimuth);
			dbg1("(sm)     ant max azimuth: %d", ref->max_azimuth);
		}
	}

	dbg1("(sm) band list");
	i = 0;
	PG_SCANLIST(&bp->band_list, brec) {
		dbg1("(sm)  band index %d", i++);
		dbg1("(sm)    name:       %s", brec->name);
		dbg1("(sm)    low:        %d", brec->low_freq);
		dbg1("(sm)    high:       %d", brec->high_freq);
		dbg1("(sm)    outputs:    %d", brec->outputs);
		dbg1("(sm)    current Rx: %d", brec->currentRx);
		dbg1("(sm)    current Tx: %d", brec->currentTx);
		dbg1("(sm)    split:      %d", brec->split);
		j = 0;
		PG_SCANLIST(&brec->ant_ref_list, ref) {
			dbg1("(sm)    ant list element %d", j++);
			dbg1("(sm)     ant index: %d", ref->idx);
		}
		j = 0;
		PG_SCANLIST(&brec->grp_ref_list, ref) {
			dbg1("(sm)    grp list element %d", j++);
			dbg1("(sm)     grp index: %d", ref->idx);
		}
	}
}

int sm_antsw_to_cfg(struct sm *sm, struct cfg *cfg) {
	struct sm_bandplan *bp;
	struct antenna_record *arec;
	struct groups_record *grec;
	struct band_record *brec;
	uint16_t i, j;
	char str[256];
	uint8_t c;

	bp = sm->bp_eeprom;

	if( ! bp) {
		return -1;
	}

	if(sm->get_antsw_state != STATE_GET_ANTSW_DONE) {
		err("(sm) %s failed, invalid state %d!", __func__, sm->get_antsw_state);
		return -1;
	}

	for(i = 0; i < ARRAY_SIZE(sm_bandplan_fixed_items); i++) {
		snprintf(str, sizeof(str), "fixed.%s", sm_bandplan_fixed_items[i].key);
		c = citem_get_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
				sizeof(bp->fixed_data), sm_bandplan_fixed_items[i].key);
		cfg_set_int_value(cfg, str, c);
	}

	for(j = 0; j < ARRAY_SIZE(output_map); j++) {
		snprintf(str, sizeof(str), "output.%s", output_map[j].label);
		int val, seq, bd;
		seq = citem_get_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
				sizeof(bp->fixed_data), "sequencerOutputs");
		bd = citem_get_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
				sizeof(bp->fixed_data), "bandDependent");
		if(seq >= 0 && bd >= 0) {
			val = ((seq > output_map[j].bit) & 1) << 1;
			val |= (bd > output_map[j].bit) & 1;
			cfg_set_int_value(cfg, str, val);
		}
	}

	// ant
	i = 0;
	PG_SCANLIST(&bp->antenna_list, arec) {
		snprintf(str, sizeof(str), "ant.%d", i);
		struct cfg *child_cfg = cfg_create_child(cfg, str);
		if(!child_cfg)
			return -1;
		cfg_set_value(child_cfg, "label", arec->label);
		cfg_set_value(child_cfg, "name", arec->name);
		cfg_set_int_value(child_cfg, "steppir", arec->steppir);
		cfg_set_int_value(child_cfg, "rxonly", arec->rxonly);
		cfg_set_int_value(child_cfg, "pa_ant_nr", arec->pa_ant_number);
		cfg_set_int_value(child_cfg, "rotator", arec->rotator);
		cfg_set_int_value(child_cfg, "rotator_offset", arec->rotator_offset);

		// output settings
		snprintf(str, sizeof(str), "ant.%d.output", i);
		child_cfg = cfg_create_child(cfg, str);
		for(j = 0; j < ARRAY_SIZE(output_map); j++) {
			cfg_set_int_value(child_cfg, output_map[j].label, (arec->output_settings >> output_map[j].bit) & 1);
		}

		i++;
	}

	// groups
	i = 0;
	PG_SCANLIST(&bp->groups_list, grec) {
		snprintf(str, sizeof(str), "group.%d", i);
		struct cfg *child_cfg = cfg_create_child(cfg, str);
		if(!child_cfg)
			return -1;
		cfg_set_value(child_cfg, "label", grec->label);
		cfg_set_value(child_cfg, "name", grec->name);
		cfg_set_int_value(child_cfg, "num_antennas", grec->num_antennas);
		cfg_set_int_value(child_cfg, "min_azimuth", grec->min_azimuth);
		cfg_set_int_value(child_cfg, "max_azimuth", grec->max_azimuth);
		//cfg_set_value(child_cfg, "flags", grec->flags);
		cfg_set_int_value(child_cfg, "virtual_rotator", ! (grec->flags & 1));

		struct reference *ref;
		j = 0;
		PG_SCANLIST(&grec->ant_ref_list, ref) {
			snprintf(str, sizeof(str), "group.%d.ant.%d", i, j++);
			struct cfg *child_cfg = cfg_create_child(cfg, str);
			if(!child_cfg)
				return -1;
			cfg_set_int_value(child_cfg, "idx", ref->idx);
			cfg_set_int_value(child_cfg, "min_azimuth", ref->min_azimuth);
			cfg_set_int_value(child_cfg, "max_azimuth", ref->max_azimuth);
		}
		i++;
	}

	// bands
	i = 0;
	PG_SCANLIST(&bp->band_list, brec) {
		snprintf(str, sizeof(str), "band.%d", i);
		struct cfg *child_cfg = cfg_create_child(cfg, str);
		if(!child_cfg)
			return -1;

		cfg_set_int_value(child_cfg, "low_freq", brec->low_freq);
		cfg_set_int_value(child_cfg, "high_freq", brec->high_freq);
		cfg_set_value(child_cfg, "name", brec->name);
		cfg_set_int_value(child_cfg, "bcd_code", brec->outputs & 0x0f);
		cfg_set_int_value(child_cfg, "key_out", (brec->outputs >> 6) & 1);
		cfg_set_int_value(child_cfg, "pa_power", (brec->outputs >> 7) & 1);


		// bpf / sequencer
		snprintf(str, sizeof(str), "band.%d.bpf_seq", i);
		child_cfg = cfg_create_child(cfg, str);
		if(!child_cfg)
			return -1;
		for(j = 0; j < ARRAY_SIZE(output_map); j++) {
			cfg_set_int_value(child_cfg, output_map[j].label, (brec->bpf_sequencer >> output_map[j].bit) & 1);
		}


		// ant list
		struct reference *ref;
		char *type;
		struct PGList *list;
		if(PG_LISTEMPTY(&bp->groups_list)) {
			type = "ant";
			list = &bp->antenna_list;
		} else {
			type = "group";
			list = &bp->groups_list;
		}

		j = 0;
		PG_SCANLIST(list, ref) {
			snprintf(str, sizeof(str), "band.%d.%s.%d", i, type, j++);
			struct cfg *child_cfg = cfg_create_child(cfg, str);
			if(!child_cfg)
				return -1;
			cfg_set_int_value(child_cfg, "idx", ref->idx);
			cfg_set_int_value(child_cfg, "rx_only", ref->flags & 1);
		}
		i++;
	}

	return 0;
}

void sm_antsw_clear_lists(struct sm *sm) {
	clear_lists(sm->bp_eeprom);
}

int sm_antsw_set_opt(struct sm *sm, const char *key, uint32_t val) {
	struct sm_bandplan *bp;
	int ret;

	if(!sm->bp_cfg)
		sm->bp_cfg = bp_create();

	bp = sm->bp_cfg;

	ret = citem_set_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data, sizeof(bp->fixed_data), key, val);

	dbg0("(mhsm) %s set antsw option %s=%d", sm->serial, key, val);

	return ret;
}

