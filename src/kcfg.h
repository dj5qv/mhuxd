/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef KCFG_H
#define KCFG_H

enum {
	RADIO_1 = 1,
	RADIO_2 = 2,
};

enum {
	MOD_CW,
	MOD_VOICE,
	MOD_DIGITAL,
};

enum {
	r1FrBase_Digital = 0,
	r1FrBase         = 0,
	r1FrBase_Cw      = 10,
	r1FrBase_Voice   = 11,

	MISC_1           = 1,
	MISC_2           = 2,
	MISC_6           = 6,
};

enum {
	PTT_1  =  (1<<0),
	PTT_2  =  (1<<1),
	PTT_SEMI_BK = (1<<2),
	PTT_QSK = (1<<3),
};

enum {
	FSK_DIDDLE  =  (1 << 1),
	FSK_UOS     =  (1 << 2),
	FSK_INVERSE =  (1 << 7),
	FSK_FLAGS   =  (FSK_DIDDLE|FSK_UOS|FSK_INVERSE),
};

enum {
	MUTE_CW     = (1 << 1),
	MUTE_FSK    = (1 << 2),
	MUTE_FLAGS  = (MUTE_CW|MUTE_FSK),
};

enum {
	RSTR_PTT    = (1 << 3),
	RSTR_CW     = (1 << 4),
	RSTR_FSK    = (1 << 5),
	RSTR_FLAGS  = (RSTR_PTT|RSTR_CW|RSTR_FSK),
};

struct mh_info;
struct config;
struct kcfg *kcfg_create(const struct mh_info *mhi);
void kcfg_destroy(struct kcfg *kcfg);
int kcfg_apply_cfg(struct kcfg *kcfg, struct config *cfg);
void kcfg_test();
struct buffer *kcfg_get_buffer(struct kcfg *kcfg);

#endif // KCFG_H
