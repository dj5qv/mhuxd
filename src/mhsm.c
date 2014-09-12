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
	int id;
	char label[MAX_LABEL_LEN + 1];
	char name[MAX_NAME_LEN + 1];
	uint32_t output_settings;
	uint8_t steppir : 1;
	uint8_t rxonly : 1;
	uint8_t pa_ant_number;
	uint8_t rotator;
	int16_t rotator_offset;
};

struct reference {
	struct PGNode node;
	int id;
	int dest_id;
	uint8_t flags;
	uint16_t min_azimuth;
	uint16_t max_azimuth;
};

struct groups_record {
	struct PGNode node;
	struct buffer raw_buf;
	int id;
	char label[MAX_LABEL_LEN + 1];
	char name[MAX_NAME_LEN + 1];
	uint8_t	num_antennas;
	struct PGList ant_ref_list;
	uint8_t flags;
	uint16_t current_azimuth;
};

struct band_record {
	struct PGNode node;
	struct buffer raw_buf;
	int id;
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
	uint8_t get_antsw_state;
	uint16_t get_antsw_offset;
	uint8_t get_antsw_to_read;
	int id_cnt_ant, id_cnt_grp, id_cnt_band, id_cnt_ref;
};

static void debug_print_antsw_values(struct sm_bandplan *bp);

struct output_map {
	const char *label;
	uint8_t bit;
};

struct output_map output_map[] = {
{ "A1", 22 },
{ "A2", 20 },
{ "A3", 18 },
{ "A4", 16 },
{ "A5", 14 },
{ "A6", 12 },
{ "A7", 10 },
{ "A8", 8 },
{ "A9", 4 },
{ "A10", 2},
{ "B1", 13 },
{ "B2", 11 },
{ "B3", 23 },
{ "B4", 9 },
{ "B5", 3 },
{ "B6", 1 },
{ "B7", 21 },
{ "B8", 19 },
{ "B9", 17 },
{ "B10", 15 },
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

// Convert 1/2700 seconds from/to milliseconds with rounding
static int32_t conv_2700_to_ms(int32_t input) {
	return (input * 1000 + 2700 / 2) / 2700;
}
static int32_t conv_ms_to_2700(int32_t input) {
	return (input * 2700 + 1000 / 2) / 1000;
}


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
	CITEM("extSerPar", 12, 7, 8),   // not implemented in SM ?
	CITEMC("antSwDelay", 13, 15, 16, conv_ms_to_2700, conv_2700_to_ms),
	CITEMC("bbmDelay", 15, 7, 8, conv_ms_to_2700, conv_2700_to_ms),
	CITEM("inhibitLead", 16, 15, 16),
	CITEM("useKeyIn", 18, 0, 1),
	CITEM("invertKeyIn", 18, 1, 1),

	/*
	CITEM("sequencer.lead.keyOut", 19,0, 16), // no lead support for keyOut
	*/
	CITEM("sequencer.tail.keyOut", 21,15, 16),

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
	CITEM("sequencer.lead.A2",  99, 15, 16),
	CITEM("sequencer.tail.A2", 101, 15, 16),
	CITEM("sequencer.lead.B7", 103, 15, 16),
	CITEM("sequencer.tail.B7", 105, 15, 16),
	CITEM("sequencer.lead.A1", 107, 15, 16),
	CITEM("sequencer.tail.A1", 109, 15, 16),
	CITEM("sequencer.lead.B3", 111, 15, 16),
	CITEM("sequencer.tail.B3", 113, 15, 16),
};

static struct sm_bandplan *bp_create() {
	struct sm_bandplan *bp = w_calloc(1, sizeof(*bp));
	PG_NewList(&bp->antenna_list);
	PG_NewList(&bp->groups_list);
	PG_NewList(&bp->band_list);
	return bp;
}
static struct antenna_record *ant_by_id(struct PGList *list, int id) {
	struct antenna_record *arec ;
	PG_SCANLIST(list, arec)
		if(arec->id == id)
			return arec;
	return NULL;
}
static struct groups_record *grp_by_id(struct PGList *list, int id) {
	struct groups_record *grec ;
	PG_SCANLIST(list, grec)
		if(grec->id == id)
			return grec;
	return NULL;
}
static struct band_record *band_by_id(struct PGList *list, int id) {
	struct band_record *brec ;
	PG_SCANLIST(list, brec)
		if(brec->id == id)
			return brec;
	return NULL;
}
static struct reference *ref_by_id(struct PGList *list, int id) {
	struct reference *ref;
	PG_SCANLIST(list, ref)
		if(ref->id == id)
			return ref;
	return NULL;
}

static struct antenna_record *arec_create(struct sm *sm, int id) {
	struct antenna_record *arec;
	arec = w_calloc(1, sizeof(*arec));
	if(id > 0) {
		if(ant_by_id(&sm->bp_eeprom->antenna_list, id)) {
			err("(mhsm) %s() failed, id %d exists!", __func__, id);
			return NULL;
		}
		arec->id = id;
		if(id > sm->id_cnt_ant)
			sm->id_cnt_ant = id;
	} else {
		arec->id = ++sm->id_cnt_ant;
	}
	return arec;
}

static struct groups_record *grec_create(struct sm *sm, int id) {
	struct groups_record *grec;
	grec = w_calloc(1, sizeof(*grec));
	if(id > 0) {
		if(grp_by_id(&sm->bp_eeprom->groups_list, id)) {
			err("(mhsm) %s() failed, id %d exists!", __func__, id);
			return NULL;
		}
		grec->id = id;
		if(id > sm->id_cnt_grp)
			sm->id_cnt_grp = id;
	} else {
		grec->id = ++sm->id_cnt_grp;
	}

	PG_NewList(&grec->ant_ref_list);
	return grec;
}

static struct band_record *brec_create(struct sm *sm, int id) {
	struct band_record *brec;
	brec = w_calloc(1, sizeof(*brec));
	if(id > 0) {
		if(band_by_id(&sm->bp_eeprom->band_list, id)) {
			err("(mhsm) %s() failed, id %d exists!", __func__, id);
			return NULL;
		}
		brec->id = id;
		if(id > sm->id_cnt_band)
			sm->id_cnt_band = id;
	} else {
		brec->id = ++sm->id_cnt_band;
	}

	PG_NewList(&brec->ant_ref_list);
	PG_NewList(&brec->grp_ref_list);
	return brec;
}

static struct reference *ref_create(struct sm *sm, int id) {
	struct reference *ref;
	ref = w_calloc(1, sizeof(*ref));
	if(id > 0) {
		ref->id = id;
		if(id > sm->id_cnt_ref)
			sm->id_cnt_ref = id;
	} else {
		ref->id = ++sm->id_cnt_ref;
	}

	return ref;
}

static int ant_id_by_idx(struct PGList *list, int idx) {
	struct antenna_record *arec ;
	int cnt = 0;
	PG_SCANLIST(list, arec) {
		if(cnt == idx)
			return arec->id;
		cnt++;
	}
	return -1;
}
static int grp_id_by_idx(struct PGList *list, int idx) {
	struct groups_record *grec ;
	int cnt = 0;
	PG_SCANLIST(list, grec) {
		if(cnt == idx)
			return grec->id;
		cnt++;
	}
	return -1;
}

static void clear_ref_list(struct PGList *list) {
	struct reference *ref;
	while(( ref = (void*)PG_FIRSTENTRY(list))) {
		PG_Remove(&ref->node);
		free(ref);
	}
}

static void clear_lists(struct sm_bandplan *bp) {
	struct antenna_record *arec;
	while((arec = (void*)PG_FIRSTENTRY(&bp->antenna_list))) {
		PG_Remove(&arec->node);
		free(arec);
	}

	struct groups_record *grec;
	while((grec = (void*)PG_FIRSTENTRY(&bp->groups_list))) {
		clear_ref_list(&grec->ant_ref_list);
		PG_Remove(&grec->node);
		free(grec);
	}

	struct band_record *brec;
	while((brec = (void*)PG_FIRSTENTRY(&bp->band_list))) {
		clear_ref_list(&brec->ant_ref_list);
		clear_ref_list(&brec->grp_ref_list);
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

static int16_t check_roffset_range(int16_t offset) {
	if(offset > 180 || offset < -180) {
		warn("(mhsm) rotator offset out of range: %d", offset);
		offset = offset > 180 ? 180 : -180;
	}
	return offset;
}

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
		arec->rotator_offset = check_roffset_range(arec->rotator_offset);
	}

	return 0;
}

static int decode_grec(struct sm *sm, struct groups_record *grec) {
	int i, c, len;

	dbg1_h("(sm) grec raw: ", grec->raw_buf.data, grec->raw_buf.size);

	struct sm_bandplan *bp = sm->bp_eeprom;

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
		struct reference *ref = ref_create(sm, 0);

		BGETC(&grec->raw_buf);
		ref->dest_id = ant_id_by_idx(&bp->antenna_list, c);

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

static int decode_brec(struct sm *sm, struct band_record *brec) {
	int i, c, len, antcnt;

	dbg1_h("(sm) raw: ", brec->raw_buf.data, brec->raw_buf.size);

	struct sm_bandplan *bp = sm->bp_eeprom;

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
		struct reference *ref = ref_create(sm, 0);
		if(ba0 > 0) {
			// linked to group
			BGETC(&brec->raw_buf); // ignore ba1
			BGETC(&brec->raw_buf); // flags;
			ref->dest_id = grp_id_by_idx(&bp->groups_list, ba0 - 1);
			ref->flags = c;
			PG_AddTail(&brec->grp_ref_list, &ref->node);
		} else {
			// linked to antenna
			BGETC(&brec->raw_buf); // ba1
			ref->dest_id = ant_id_by_idx(&bp->antenna_list, c);;
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
	free(sm);
}

static void get_antsw_completion_cb(unsigned const char *reply_buf, int len, int result, void *user_data)  {
	(void)reply_buf; (void)len;
	struct sm *sm = user_data;
	struct sm_bandplan *bp;
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

	bp = sm->bp_eeprom;
	if(!bp)
		return;

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
			bp->fixed_data[sm->get_antsw_offset + i] = c;
			sm->get_antsw_to_read--;
			if(sm->get_antsw_to_read == 0)
				sm->get_antsw_state = STATE_GET_ANTSW_ANTREC_LENTGH;
			break;

		case STATE_GET_ANTSW_ANTREC_LENTGH:
			if(c > 0) {
				sm->get_antsw_state = STATE_GET_ANTSW_ANTREC;
				sm->get_antsw_to_read = c;
				arec = arec_create(sm, 0);
				PG_AddTail(&bp->antenna_list, &arec->node);
			} else {
				sm->get_antsw_state = STATE_GET_ANTSW_GRPREC_LENTGH;
			}
			break;

		case STATE_GET_ANTSW_ANTREC:
			arec = (void*)PG_LASTENTRY(&bp->antenna_list);
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
				grec = grec_create(sm, 0);
				PG_AddTail(&bp->groups_list, &grec->node);
			} else {
				sm->get_antsw_state = STATE_GET_ANTSW_BREC_LENTGH;
			}
			break;

		case STATE_GET_ANTSW_GRPREC:
			grec = (void*)PG_LASTENTRY(&bp->groups_list);
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
				brec = brec_create(sm, 0);
				PG_AddTail(&bp->band_list, &brec->node);
			} else {
				sm->get_antsw_state = STATE_GET_ANTSW_DONE;
			}
			break;

		case STATE_GET_ANTSW_BREC:
			brec = (void*)PG_LASTENTRY(&bp->band_list);
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

		citem_debug_print_values("sm antsw fixed", sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data, 115);

		PG_SCANLIST(&bp->antenna_list, arec) {
			dbg1("(sm) decode arec %d", i);
			if(-1 == decode_arec(arec))
				err("(sm) error decoding antenna record!");
			i++;
		}
		i = 0;
		PG_SCANLIST(&bp->groups_list, grec) {
			dbg1("(sm) decode grec %d", i);
			if(-1 == decode_grec(sm, grec))
				err("(sm) error decoding groups record!");
			i++;
		}
		i = 0;
		PG_SCANLIST(&bp->band_list, brec) {
			dbg1("(sm) decode brec %d", i);
			if(-1 == decode_brec(sm, brec))
				err("(sm) error decoding band record!");
			i++;
		}

		debug_print_antsw_values(bp);
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
		dbg1("(sm)    id:          %u", arec->id);
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
			dbg1("(sm)     ant id:          %d", ref->id);
			dbg1("(sm)     ant dest_id:     %d", ref->dest_id);
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
			dbg1("(sm)     ant id:      %d", ref->id);
			dbg1("(sm)     ant dest_id: %d", ref->dest_id);
		}
		j = 0;
		PG_SCANLIST(&brec->grp_ref_list, ref) {
			dbg1("(sm)    grp list element %d", j++);
			dbg1("(sm)     grp id:      %d", ref->id);
			dbg1("(sm)     grp dest_id: %d", ref->dest_id);
		}
	}
}

int sm_antsw_to_cfg(struct sm *sm, struct cfg *cfg) {
	struct sm_bandplan *bp;
	struct antenna_record *arec;
	struct groups_record *grec;
	struct band_record *brec;
	uint16_t j;
	char str[256];

	bp = sm->bp_eeprom;

	if(!bp)
		return -1;

	dbg1("(mhsm) %s()", __func__);

	if(sm->get_antsw_state != STATE_GET_ANTSW_DONE && sm->get_antsw_state != STATE_GET_ANTSW_EMPTY) {
		// GET ANTSW pending
		err("(sm) %s failed, invalid state %d!", __func__, sm->get_antsw_state);
		return -1;
	}


	for(j = 0; j < ARRAY_SIZE(sm_bandplan_fixed_items); j++) {
		snprintf(str, sizeof(str), "fixed.%s", sm_bandplan_fixed_items[j].key);
		int c = citem_get_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
				sizeof(bp->fixed_data), sm_bandplan_fixed_items[j].key);
		if(c != -1)
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
			val = ((seq >> output_map[j].bit) & 1) << 1;
			val |= (bd >> output_map[j].bit) & 1;
			cfg_set_int_value(cfg, str, val);
		}
	}

	// ant
	PG_SCANLIST(&bp->antenna_list, arec) {
		snprintf(str, sizeof(str), "ant.%d", arec->id);
		struct cfg *child_cfg = cfg_create_child(cfg, str);
		if(!child_cfg)
			return -1;
		cfg_set_value(child_cfg, "label", arec->label);
		cfg_set_value(child_cfg, "display", arec->name);
		cfg_set_int_value(child_cfg, "id", arec->id);
		cfg_set_int_value(child_cfg, "steppir", arec->steppir);
		cfg_set_int_value(child_cfg, "rxonly", arec->rxonly);
		cfg_set_int_value(child_cfg, "pa_ant_number", arec->pa_ant_number);
		cfg_set_int_value(child_cfg, "rotator", arec->rotator);
		cfg_set_int_value(child_cfg, "rotator_offset", arec->rotator_offset);

		// output settings
		snprintf(str, sizeof(str), "ant.%d.output", arec->id);
		child_cfg = cfg_create_child(cfg, str);
		for(j = 0; j < ARRAY_SIZE(output_map); j++) {
			cfg_set_int_value(child_cfg, output_map[j].label, (arec->output_settings >> output_map[j].bit) & 1);
		}
	}

	// groups
	PG_SCANLIST(&bp->groups_list, grec) {
		snprintf(str, sizeof(str), "group.%d", grec->id);
		struct cfg *child_cfg = cfg_create_child(cfg, str);
		if(!child_cfg)
			return -1;
		cfg_set_value(child_cfg, "label", grec->label);
		cfg_set_value(child_cfg, "display", grec->name);
		cfg_set_int_value(child_cfg, "id", grec->id);
		cfg_set_int_value(child_cfg, "num_antennas", grec->num_antennas);
		//cfg_set_value(child_cfg, "flags", grec->flags);
		cfg_set_int_value(child_cfg, "virtual_rotator", ! (grec->flags & 1));

		struct reference *ref;
		PG_SCANLIST(&grec->ant_ref_list, ref) {
			snprintf(str, sizeof(str), "group.%d.ant.%d", grec->id, ref->id);
			struct cfg *child_cfg = cfg_create_child(cfg, str);
			if(!child_cfg)
				return -1;
			cfg_set_int_value(child_cfg, "id", ref->id);
			cfg_set_int_value(child_cfg, "dest_id", ref->dest_id);
			cfg_set_int_value(child_cfg, "min_azimuth", ref->min_azimuth);
			cfg_set_int_value(child_cfg, "max_azimuth", ref->max_azimuth);
		}
	}

	// bands
	PG_SCANLIST(&bp->band_list, brec) {
		snprintf(str, sizeof(str), "band.%d", brec->id);
		struct cfg *child_cfg = cfg_create_child(cfg, str);
		if(!child_cfg)
			return -1;

		cfg_set_int_value(child_cfg, "id", brec->id);
		cfg_set_int_value(child_cfg, "low_freq", brec->low_freq);
		cfg_set_int_value(child_cfg, "high_freq", brec->high_freq);
		cfg_set_value(child_cfg, "display", brec->name);
		cfg_set_int_value(child_cfg, "bcd_code", brec->outputs & 0x0f);
		cfg_set_int_value(child_cfg, "key_out", (brec->outputs >> 6) & 1);
		cfg_set_int_value(child_cfg, "pa_power", (brec->outputs >> 7) & 1);


		// bpf / sequencer
		snprintf(str, sizeof(str), "band.%d.bpf_seq", brec->id);
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

		PG_SCANLIST(list, ref) {
			snprintf(str, sizeof(str), "band.%d.%s.%d", brec->id, type, ref->id);
			struct cfg *child_cfg = cfg_create_child(cfg, str);
			if(!child_cfg)
				return -1;
			cfg_set_int_value(child_cfg, "id", ref->id);
			cfg_set_int_value(child_cfg, "dest_id", ref->dest_id);
			cfg_set_int_value(child_cfg, "rx_only", ref->flags & 1);
		}
	}

	return 0;
}

void sm_antsw_clear_lists(struct sm *sm) {
	dbg1("(mhsm) %s()", __func__);
	clear_lists(sm->bp_eeprom);
}

int sm_antsw_add_ant(struct sm *sm, struct cfg *cfg) {
	struct antenna_record *arec;
	struct sm_bandplan *bp = sm->bp_eeprom;
	if(!bp)
		return -1;

	dbg1("(mhsm) %s()", __func__);

	arec = arec_create(sm, cfg_name_to_int(cfg, 0));
	PG_AddTail(&bp->antenna_list, &arec->node);
	cfg_set_int_value(cfg, "id", arec->id);
	return sm_antsw_mod_ant(sm, cfg);
}

int sm_antsw_mod_ant(struct sm *sm, struct cfg *cfg) {
	struct antenna_record *arec;
	struct sm_bandplan *bp = sm->bp_eeprom;
	const char *str;
	if(!bp)
		return -1;

	dbg1("(mhsm) %s()", __func__);

	int id = cfg_get_int_val(cfg, "id", -1);
	arec = ant_by_id(&bp->antenna_list, id);
	if(!arec) {
		err("(mhsm) %s() failed, antenna id %d not found!", __func__, id);
		return -1;
	}

	str = cfg_get_val(cfg, "label", arec->label);
	strncpy(arec->label, str, MAX_LABEL_LEN);

	str = cfg_get_val(cfg, "display", arec->name);
	strncpy(arec->name, str, MAX_NAME_LEN);

	arec->steppir = cfg_get_int_val(cfg, "steppir", arec->steppir);
	arec->rxonly = cfg_get_int_val(cfg, "rxonly", arec->rxonly);
	arec->pa_ant_number = cfg_get_int_val(cfg, "pa_ant_number", arec->pa_ant_number);
	arec->rotator = cfg_get_int_val(cfg, "rotator", arec->rotator);
	arec->rotator_offset = cfg_get_int_val(cfg, "rotator_offset", arec->rotator_offset);
	arec->rotator_offset = check_roffset_range(arec->rotator_offset);

	// output bits
	uint16_t j;
	int val;
	cfg = cfg_get_child(cfg, "output");
	arec->output_settings = 0;
	for(j = 0; cfg && j < ARRAY_SIZE(output_map); j++) {
		val = cfg_get_int_val(cfg, output_map[j].label, (arec->output_settings >> output_map[j].bit) & 1);
		val = (val == 1 ? 1 : 0);
		arec->output_settings |= (val << output_map[j].bit);
	}

	return 0;
}

int sm_antsw_add_group(struct sm *sm, struct cfg *cfg) {
	struct groups_record *grec;
	struct sm_bandplan *bp = sm->bp_eeprom;
	if(!bp)
		return -1;

	dbg1("(mhsm) %s()", __func__);

	grec = grec_create(sm, cfg_name_to_int(cfg, 0));
	PG_AddTail(&bp->groups_list, &grec->node);
	cfg_set_int_value(cfg, "id", grec->id);
	return sm_antsw_mod_group(sm, cfg);
}

int sm_antsw_mod_group(struct sm *sm, struct cfg *cfg) {
	struct groups_record *grec;
	struct sm_bandplan *bp = sm->bp_eeprom;
	const char *str;
	if(!bp)
		return -1;

	dbg1("(mhsm) %s()", __func__);

	int id = cfg_get_int_val(cfg, "id", -1);
	if(id == -1)
		id = cfg_name_to_int(cfg, -1);

	grec = grp_by_id(&bp->groups_list, id);
	if(!grec) {
		err("(mhsm) %s() failed, group id %d not found!", __func__, id);
		return -1;
	}

	str = cfg_get_val(cfg, "label", grec->label);
	if(str != grec->label) // may point to the same address 
		strncpy(grec->label, str, MAX_LABEL_LEN);

	str = cfg_get_val(cfg, "display", grec->name);
	if(str != grec->name) // may point to the same address 
		strncpy(grec->name, str, MAX_NAME_LEN);

	grec->num_antennas = cfg_get_int_val(cfg, "num_antennas", grec->num_antennas);
	int c = cfg_get_int_val(cfg, "virtual_rotator", -1);
	if(c != -1) {
		grec->flags &= ~1;
		grec->flags |= !c;
	}
	
	//clear_ref_list(&grec->ant_ref_list);

	struct cfg *pcfg;
	struct reference *ref;

	for(pcfg = cfg_first_child( cfg_get_child(cfg, "ant")); pcfg; pcfg = cfg_next_child(pcfg)) {
		ref = ref_by_id(&grec->ant_ref_list,  cfg_name_to_int(pcfg, -1));
		if(!ref) {
			ref = ref_create(sm, cfg_name_to_int(pcfg, 0));
			PG_AddTail(&grec->ant_ref_list, &ref->node);
		}

		ref->dest_id = cfg_get_int_val(pcfg, "dest_id", -1);
		ref->min_azimuth = cfg_get_int_val(pcfg, "min_azimuth", 0);
		ref->max_azimuth = cfg_get_int_val(pcfg, "max_azimuth", 0);
	}

	return 0;
}

int sm_antsw_rem_ant(struct sm *sm, int id) {

	struct antenna_record *arec;
	struct sm_bandplan *bp = sm->bp_eeprom;

	if(!bp)
		return -1;

	dbg1("(mhsm) %s() id: %d", __func__, id);
	//debug_print_antsw_values(sm->bp_eeprom);

	// FIXME: check references to this antenna and remove them.

	arec = ant_by_id(&bp->antenna_list, id);
	if(arec) {
		PG_Remove(&arec->node);
		free(arec);
		return 0;
	}

	err("(mhsm) could not remove antenna id %d, not found!", id);
	return -1;
}

int sm_antsw_rem_group(struct sm *sm, int id) {

	struct groups_record *grec;
	struct sm_bandplan *bp = sm->bp_eeprom;

	if(!bp)
		return -1;

	dbg1("(mhsm) %s() id: %d", __func__, id);

	grec = grp_by_id(&bp->groups_list, id);
	if(grec) {
		clear_ref_list(&grec->ant_ref_list);
		PG_Remove(&grec->node);
		free(grec);
		return 0;
	}


	err("(mhsm) could not remove group id %d, not found!", id);
	return -1;
}

int sm_antsw_rem_group_ref(struct sm *sm, int grp_id, int ref_id) {
	struct groups_record *grec;

	struct sm_bandplan *bp = sm->bp_eeprom;

	if(!bp)
		return -1;

	dbg1("(mhsm) %s() gid: %d rid %d", __func__, grp_id, ref_id);

	grec = grp_by_id(&bp->groups_list, grp_id);
	if(grec) {
		struct reference *ref = ref_by_id(&grec->ant_ref_list, ref_id);
		if(ref) {
			PG_Remove(&ref->node);
			free(ref);
			return 0;
		}
	}

	err("(mhsm) could not remove ref id %d in group id %d, not found!", ref_id, grp_id);
	return -1;

}

int sm_antsw_set_opt(struct sm *sm, const char *key, uint32_t val) {
	struct sm_bandplan *bp;
	int ret;

	if(!sm->bp_eeprom)
		sm->bp_eeprom = bp_create();

	bp = sm->bp_eeprom;

	ret = citem_set_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data, sizeof(bp->fixed_data), key, val);

	dbg0("(mhsm) %s set antsw option %s=%d", sm->serial, key, val);

	return ret;
}

int sm_antsw_set_output(struct sm *sm, const char *out_name, uint8_t val) {
	uint16_t j;
	int seq, bd;
	struct sm_bandplan *bp = sm->bp_eeprom;

	if(!bp)
		return -1;

	dbg0("(mhsm) %s set antsw output %s=%d", sm->serial, out_name, val);

	if(val >= SM_OUT_MAX) {
		err("(mhsm) %s invalid value %u", __func__, val);
		return -1;
	}

	for(j = 0; j < ARRAY_SIZE(output_map); j++) {
		if(!strcmp(output_map[j].label, out_name)) {

			seq = citem_get_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
					sizeof(bp->fixed_data), "sequencerOutputs");
			bd = citem_get_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
					sizeof(bp->fixed_data), "bandDependent");
			if(seq >= 0 && bd >= 0) {
				seq &= ~(1 << output_map[j].bit);
				bd  &= ~(1 << output_map[j].bit);
				seq |= (((val >> 1) & 1) << output_map[j].bit);
				bd  |= (((val >> 0) & 1) << output_map[j].bit);

				citem_set_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
						sizeof(bp->fixed_data), "sequencerOutputs", seq);
				citem_set_value(sm_bandplan_fixed_items, ARRAY_SIZE(sm_bandplan_fixed_items), bp->fixed_data,
						sizeof(bp->fixed_data), "bandDependent", bd);

			} else {
				return -1;
			}

			return 0;
		}
	}

	err("(mhsm) %s could not find output relay %s!", __func__, out_name);
	return -1;
}

int sm_antsw_has_bandplan(struct sm *sm) {
	return sm->bp_eeprom ? 1 : 0;
}
