/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdint.h>
#include <string.h>
#include "kcfg.h"
#include "global.h"
#include "util.h"
#include "buffer.h"
#include "mhinfo.h"
#include "logger.h"
#include "cfgfile.h"

static int conv_audiosw_ptt(const char *val, unsigned *out_result);

struct kcfg {
	struct buffer b;
	const struct mh_info *mhi;
};

struct citem {
	const char	*key;
	uint8_t		off;
	uint8_t		base_bit;
	uint8_t		width;
	uint32_t	def;
	int (*conv_func)(const char *val, unsigned *out_result);
};

static const struct citem citems[] = {
	{ "r1FrBase",	        0,  7, 8, 0xEA, conv_audiosw_ptt }, /* U2R, MK */
	{ "r1FrBase_Digital",	0,  7, 8, 0xEA, conv_audiosw_ptt }, /* MK2, DK2, MK2R */

	{ "r1DelaySerialPtt",	1,  0, 1,  1, NULL },
	{ "fskDiddle",		1,  1, 1,  1, NULL },
	{ "fskUos",		1,  2, 1,  1, NULL },
	{ "restorePtt",		1,  3, 1,  1, NULL },
	{ "restoreCompCw",	1,  4, 1,  1, NULL },
	{ "restoreCompFsk",	1,  5, 1,  1, NULL },
	{ "r1LnaPtt",		1,  6, 1,  1, NULL },
	{ "r1InvertFsk",	1,  7, 1,  0, NULL },

	{ "r1PaPtt",		2,  0, 1,  1, NULL },
	{ "muteCompCw",		2,  1, 1,  0, NULL },
	{ "muteCompFsk",	2,  2, 1,  0, NULL },
	{ "r1I2cCoupled",	2,  7, 1,  0, NULL },

	{ "r1PttDelay",		3,  7, 8,  1, NULL },

	{ "r1InvertLnaPtt",	4,  0, 1,  0, NULL },
	{ "r1ImmediateCw",	4,  1, 1,  0, NULL },
	{ "r1TrDelay",		4,  7, 6,  0, NULL },

	{ "sideTone",		5,  2, 3,  0, NULL },
	{ "speedStep",		5,  7, 4,  2, NULL },

	{ "r1ForceKeyerMode",	6,  0, 1,  1, NULL },
	{ "fskTypeAhead",	6,  1, 1,  0, NULL },
	{ "cwTypeAhead",	6,  2, 1,  0, NULL },
	{ "r1UseLnaPttTail",	6,  3, 1,  1, NULL },
	{ "r1FollowTxMode",	6,  4, 1,  1, NULL },
	{ "r1AllowCwInVoice",	6,  5, 1,  0, NULL },
	{ "qwertz",		6,  6, 1,  0, NULL },
	{ "r1Qsk",		6,  7, 1,  0, NULL },

	{ "r1LineOutRight",	7,  0, 1,  0, NULL },
	{ "numLeadZeroT",	7,  1, 1,  0, NULL },
	{ "numZeroT",		7,  2, 1,  0, NULL },
	{ "numOneA",		7,  3, 1,  0, NULL },
	{ "numNineN",		7,  4, 1,  0, NULL },
	{ "r1KeyerMode",	7,  6, 2,  0, NULL },
	{ "numReport5NN",	7,  7, 1,  0, NULL },

	{ "r1PaPttTail",	8,  7, 8,  1, NULL },
	{ "r1LnaPttTail,",	9,  7, 8,  1, NULL },
	{ "r1FrBase_Cw",	10, 7, 8,  0xEA, conv_audiosw_ptt }, /* AAA, PTT1|PTT2 */
	{ "r1FrBase_Voice",	11, 7, 8,  0xEA, conv_audiosw_ptt }, /* AAA, PTT1|PTT2 */
};

struct citem citems_dk2[] = {
	{ "useAutoPtt",		28, 2, 1,  0, NULL },
	{ "paddleOnlySideTone",	28, 3, 1,  0, NULL },
	{ "downstreamOverFootSw",28, 5, 1,  0, NULL },
	{ "useQCw",             28, 6, 1,  0, NULL },
	{ "usePFsk",            28, 7, 1,  0, NULL },
	/* X30-X39 = keyFuncPs2[0] - keyFuncPs2[9] */
	{ "civAddress",		40, 7, 8,  0x70, NULL }, /* IC-7000 */
	{ "civBaudRate",	41, 7, 8,  0x24, NULL }, /* 9600 */
	{ "civFunc",		42, 7, 8,  0x00, NULL }, /* none */
	/* X44-X55 = keyFuncFh2[0] - keyFuncFh2[11] */
};

struct citem citems_mk2[] = {
	{ "r1FrMpkExtra_Digital",12, 7, 8,  0, NULL },
	{ "pwmContrast",	13, 4, 5, 12, NULL },
	{ "pwmBlight",		14, 4, 5, 12, NULL },
	{ "dispBg[0]",		15, 7, 8, 0x06, NULL }, /* RX freq */
	{ "dispBg[1]",		15, 7, 8, 0x05, NULL }, /* WPM / speed pot */
	/* off 17 - 25 to be implemented seperately (dispEv / dispEvLn) */
	{ "dispEvTime",		25, 7, 8, 200, NULL },
	{ "r1FrMpkExtra_Cw",	26, 7, 8,  0, NULL },
	{ "r1FrMpkExtra_Voice",	27, 7, 8,  0, NULL },

	{ "micSelAuto",		28, 0, 1,  0, NULL },
	{ "micSelFront",	28, 1, 1,  0, NULL },
	{ "useAutoPtt",		28, 2, 1,  0, NULL },
	{ "paddleOnlySideTone",	28, 3, 1,  0, NULL },
	{ "enableSleepMode",	28, 4, 1,  0, NULL },
	{ "downstreamOverFootSw",28, 5, 1,  0, NULL },
	{ "extSerBaudRate",	29, 7, 8,  24, NULL }, /* 9600 */
	/* off 30 - 39 keyFuncPs[0-9] to be implemented seperately. */

	{ "civAddress",		40, 7, 8,  0x70, NULL }, /* IC-7000 */
	{ "civBaudRate",	41, 7, 8,  0x24, NULL }, /* 9600 */
	{ "civFunc",		42, 7, 8,  0x00, NULL }, /* none */
	/* 43 reserve */
	/* X44-X55 = keyFuncFh2[0] - keyFuncFh2[11], to be implemented seperately */
};

struct citem citems_mk2r[] = {
	{ "r1FrMokExtra_Digital", 12, 7, 8, 0, NULL },
	{ "r2FrBase_Digital",     13, 7, 8, 0xEA, conv_audiosw_ptt },
	{ "r2FrMokExtra_Digital", 14, 7, 8, 0, NULL },

	{ "r2PaPtt",             15, 0, 1, 1, NULL },
	{ "singleSerialCw",      15, 1, 1, 0, NULL },
	{ "singleSerialPtt",     15, 2, 1, 0, NULL },
	{ "singleFootSwitch",    15, 7, 1, 0, NULL },

	{ "r2DelaySerialPtt",    16, 0, 1, 1, NULL },
	{ "r2I2cCoupled",        16, 1, 1, 0, NULL },
	{ "lastOneWins",         16, 2, 1, 0, NULL },
	{ "dualTx",              16, 3, 1, 0, NULL },
	{ "useLptPtt",           16, 4, 1, 0, NULL },
	{ "useLptCw",            16, 5, 1, 0, NULL },
	{ "r2LnaPtt",            16, 6, 1, 1, NULL },
	{ "r2InvertFsk",         16, 7, 1, 0, NULL },

	{ "r2LineOutRight",      17, 0, 1, 0, NULL },
	{ "lptDvk",              17, 1, 1, 0, NULL },
	{ "disableBandLock",     17, 4, 1, 0, NULL },
	{ "r2KeyerMode",         17, 6, 2, 0x00, NULL },
	{ "useLptFsIn",          17, 7, 1, 0, NULL },

	{ "r2ForceKeyerMode",    18, 0, 1, 0, NULL },
	{ "singleSerialFsk,",    18, 1, 1, 0, NULL },
	{ "useChanFocus",        18, 2, 1, 0, NULL },
	{ "r2UseLnaPttTail",     18, 3, 1, 1, NULL },
	{ "r2FollowTxMode",      18, 4, 1, 0, NULL },
	{ "r2AllowCwInVoice",    18, 5, 1, 0, NULL },
	{ "singleWinkeyCwPtt",   18, 6, 1, 0, NULL },
	{ "r2Qsk",               18, 7, 1, 0, NULL },

	{ "r2PttDelay",          19, 7, 8, 0, NULL },
	{ "modeA",               20, 7, 4, 0, NULL },
	{ "modeB",               21, 3, 4, 0, NULL },
	{ "modeC",               21, 7, 4, 0, NULL },

	{ "txFocusAutoCmd",      22, 0, 1, 0, NULL },
	{ "rxFocusAutoCmd",      22, 1, 1, 0, NULL },
	{ "stereoFocusAutoCmd",  22, 2, 1, 0, NULL },
	{ "txFocusAuto2Ptt",     22, 3, 1, 0, NULL },
	{ "rxFocusFollowTx",     22, 4, 1, 0, NULL },
	{ "stereoFocusNone",     22, 5, 1, 0, NULL },
	{ "txFocusPinInverted",  23, 0, 1, 0, NULL },
	{ "rxFocusPinInverted",  23, 1, 1, 0, NULL },
	{ "stereoFocusPinInverted",  23, 2, 1, 0, NULL },
	{ "txFocusPin3",         23, 3, 1, 0, NULL },
	{ "stereoFocusPin5",     23, 4, 1, 0, NULL },

	{ "ser1Source",          24, 1, 2, 0, NULL },
	{ "r1OutputDataType",    24, 6, 3, 0, NULL },
	{ "r1BandDataInput",     24, 7, 1, 0, NULL },
	{ "pin4Function",        24, 11, 4, 0, NULL },
	{ "pin5Function",        24, 15, 1, 0, NULL },
	{ "ser2Source",          24, 17, 2, 0, NULL },
	{ "r2OutputDataType",    24, 22, 3, 0, NULL },
	{ "r2BandDataInput",     24, 23, 1, 0, NULL },
	{ "pin3Function",        24, 27, 4, 0, NULL },
	{ "pin2Function",        24, 31, 4, 0, NULL },

	{ "audioSwitchingDelay", 28, 7, 8, 0, NULL },
	{ "r2InvertLnaPtt",      29, 0, 1, 0, NULL },
	/* X30-X39 = keyFuncPs2[0] - keyFuncPs2[9] */
	{ "r1FrMokExtra_Cw",     40, 7, 8, 0, NULL },
	{ "r2FrBase_Cw",         41, 7, 8, 0xEA, conv_audiosw_ptt },
	{ "r2FrMokExtra_Cw",     42, 7, 8, 0, NULL },
	{ "r1FrMokExtra_Voice",  43, 7, 8, 0, NULL },
	{ "r2FrBase_Voice",      44, 7, 8, 0xEA, conv_audiosw_ptt },
	{ "r2FrMokExtra_Voice",  45, 7, 8, 0, NULL },

	{ "accSer1Func",         46, 7, 8, 0, NULL },
	{ "accSer2Func",         47, 7, 8, 0, NULL },

	{ "accSer1BaudRate",     48, 7, 8, 0x24, NULL },
	{ "accSer1BaudRate",     49, 7, 8, 0x24, NULL },

	{ "accSer1Par",          50, 7, 8, 0, NULL },
	{ "accSer2Par",          51, 7, 8, 0, NULL },

	{ "r2PaPttTail",         52, 7, 8, 0, NULL },
	{ "r2LnaPttTail",        53, 7, 8, 0, NULL },
};

struct citem citems_u2r[] = {
	{ "r2FrBase",	        14,  7, 8, 0xEA, conv_audiosw_ptt },
	{ "singleSerialCw",     15,  1, 1, 0, NULL },
	{ "singleSerialPtt",    15,  2, 1, 0, NULL },
	{ "r2DelaySerialPtt",   16,  1, 1, 0, NULL },
	{ "lastOneWins",        16,  2, 1, 0, NULL },
	{ "dualTx",             16,  3, 1, 0, NULL },
	{ "useLptPtt",          16,  4, 1, 0, NULL },
	{ "useLptCw",           16,  5, 1, 0, NULL },
	{ "r2InvertFsk",        16,  7, 1, 0, NULL },

	{ "disableBandLock",    17,  4, 1, 0, NULL },
	{ "r2KeyerMode",        17,  6, 2, 0, NULL },

	{ "r2ForceKeyerMode",   18,  0, 1, 0, NULL },
	{ "singleSerialFsk",    18,  1, 1, 0, NULL },
	{ "r2AllowCwInVoice",   18,  5, 1, 0, NULL },
	{ "singleWinkeyCwPtt",  18,  6, 1, 0, NULL },

	{ "r2PttDelay",         19,  7, 8, 0, NULL },

	{ "txFocusAutoCmd",      22, 0, 1, 0, NULL },
	{ "rxFocusAutoCmd",      22, 1, 1, 0, NULL },
	{ "stereoFocusAutoCmd",  22, 2, 1, 0, NULL },
	{ "txFocusAuto2Ptt",     22, 3, 1, 0, NULL },
	{ "rxFocusFollowTx",     22, 4, 1, 0, NULL },
	{ "stereoFocusNone",     22, 5, 1, 0, NULL },
	{ "txFocusPinInverted",  23, 0, 1, 0, NULL },
	{ "rxFocusPinInverted",  23, 1, 1, 0, NULL },
	{ "stereoFocusPinInverted",  23, 2, 1, 0, NULL },
	{ "txFocusPin3",         23, 3, 1, 0, NULL },
	{ "stereoFocusPin5",     23, 4, 1, 0, NULL },

	{ "audioSwitchingDelay", 28, 7, 8, 0, NULL },
	{ "r2InvertLnaPtt",      29, 0, 1, 0, NULL },
	/* X30-X39 = keyFuncPs2[0] - keyFuncPs2[9] */

	{ "rxFocus_r1RxRx",      40, 0, 1, 0, NULL },
	{ "split_r1RxRx",        40, 1, 1, 0, NULL },
	{ "rxFocus_r1TxRxComputer",40, 2, 1, 0, NULL },
	{ "split_r1TxRxComputer",40, 3, 1, 0, NULL },
	{ "rxFocus_r2RxRx",      40, 4, 1, 0, NULL },
	{ "split_r2RxRx",        40, 5, 1, 0, NULL },
	{ "rxFocus_r2RxTxComputer",40, 6, 1, 0, NULL },
	{ "split_r2RxTxComputer",40, 7, 1, 0, NULL },
	{ "rxFocus_earsStereo",  40, 8, 1, 0, NULL },
	{ "split_earsStereo",    40, 9, 1, 0, NULL },
	{ "rxFocus_r1TxRxManual",40,12, 1, 0, NULL },
	{ "split_r1TxRxManual",  40,13, 1, 0, NULL },
	{ "rxFocus_r2RxTxManual",40,14, 1, 0, NULL },
	{ "split_r2RxTxManual",  40,15, 1, 0, NULL },
	/* X42 = u2rFlags ???? */
	/* X44-X55 = keyFuncFh2[0] - keyFuncFh2[11] */

};

static int kcfg_set_byte(struct kcfg *kcfg, unsigned idx, unsigned char val) {
	if(idx >= kcfg->b.size)
		return -1;
	kcfg->b.data[idx] = val;
	return 0;
}

static int kcfg_get_byte(struct kcfg *kcfg, unsigned idx) {
	if(idx >= kcfg->b.size)
		return -1;
	return kcfg->b.data[idx];
}

static int get_citem_ext(int type, const struct citem **cbasep, unsigned *sizep) {
	switch(type) {

	case MHT_DK2:
		*cbasep = citems_dk2;
		*sizep = ARRAY_SIZE(citems_dk2);
		break;
	case MHT_MK2:
		*cbasep = citems_mk2;
		*sizep = ARRAY_SIZE(citems_mk2);
		break;
	case MHT_MK2R:
	case MHT_MK2Rp:
		*cbasep = citems_mk2r;
		*sizep = ARRAY_SIZE(citems_mk2r);
		break;
	case MHT_U2R:
		*cbasep = citems_u2r;
		*sizep = ARRAY_SIZE(citems_u2r);
		break;
	case MHT_CK:
	case MHT_DK:
	case MHT_MK:
	case MHT_SM:
	case MHT_SMD:
	default:
		*cbasep = NULL;
		*sizep = 0;
	}

	return 0;
}

static const struct citem *find_citem(struct kcfg *kcfg,  const char *key) {
	const struct citem *cp = NULL;
	unsigned i;

	for(i = 0; i < ARRAY_SIZE(citems); i++) {
		if(!strcmp(key, citems[i].key)) {
			cp = &citems[i];
			break;
		}
	}

	if(cp)
		return cp;

	const struct citem *cbase;
	unsigned  size;

	get_citem_ext(kcfg->mhi->type, &cbase, &size);
	if(!cbase)
		return NULL;

	for(i = 0; i < size; i++) {
		if(!strcmp(key, cbase[i].key)) {
			cp = &cbase[i];
			break;
		}
	}

	return cp;
}

static uint8_t width2mask(int w) {
	return (0xff >> (8-w));
}

static int kcfg_set_val(struct kcfg *kcfg, const char *key, int val) {
	const struct citem *cp;
	int c;
	int idx, bit, mask;

	cp = find_citem(kcfg, key);
	if(!cp) {
		warn("KCFG Could not find config key: %s", key);
		return -1;
	}

	idx = cp->off + cp->base_bit / 8;
	bit = cp->base_bit % 8;
	mask = width2mask(cp->width);

	c = kcfg_get_byte(kcfg, idx) ;
	if(c < 0) {
		err("KCFG kcfg_set_val() index %d out of range!", idx);
		return -1;
	}

	if(val > mask) {
		err("KCFG kcfg_set_val() Invalid value %d for %s", val, key);
		return -1;
	}

	c &= ~(mask << (bit + 1 - cp->width));
	c |=  (val << (bit + 1 - cp->width));

	kcfg_set_byte(kcfg, idx, c);

	return 0;
}

static int get_frbase_idx(struct kcfg *kcfg, int mode, int radio) {
	int idx = -1;
	switch(kcfg->mhi->type) {
	case MHT_CK:
	case MHT_DK:
	case MHT_MK:
		idx = 0;
		break;
	case MHT_U2R:
		idx = (radio == RADIO_1) ? 0 : 13;
		break;
	default:
		switch(mode) {
		case MOD_DIGITAL:
			idx = (radio == RADIO_1) ? 0 : 13;
			break;
		case MOD_CW:
			idx = (radio == RADIO_1) ? 10 : 41;
			break;
		case MOD_VOICE:
			idx = (radio == RADIO_1) ? 11: 44;
			break;
		}
	}
	if(idx < 0) {
		err("KCFG get_frbase_idx() could not calculate index!");
		return -1;
	}
	return idx;
}

#if 0
/* Set audio switching e.g. AAA, ACA etc.
 * (r1/2FrBase, r1/2FrBase_XXX).
 */
int kcfg_set_audio_switching(struct kcfg *kcfg, unsigned radio, int mode, char audio_char[3]) {
	int idx = -1;
	int i;
	unsigned char c, bits;

	idx = get_frbase_idx(kcfg, mode, radio);
	if(idx >= (int)kcfg->b.size || idx < 0) {
		err("KCFG kcfg_set_audio_switching() index %d out of range!", idx);
		return -1;
	}

	c = kcfg_get_byte(kcfg, idx);
	for(i = 0; i < 3; i++) {
		bits = audiobyte_to_bits(audio_char[i], (c >> (i*2)) & 0x03);
		c &= ~(0x03 << (2*i));
		c |= (bits << (2*i));
		kcfg_set_byte(kcfg, idx, c);
		info("KCFG set audio byte %d to 0x%02x", idx, c);
	}

	return 0;
}

int kcfg_set_ptt(struct kcfg *kcfg, int mode, int radio, int ptt) {
	unsigned char c;
	int idx;

	idx = get_frbase_idx(kcfg, mode, radio);
	if(idx >= (int)kcfg->b.size || idx < 0) {
		err("KCFG kcfg_set_ptt() index %d out of range!", idx);
		return -1;
	}

	c = kcfg_get_byte(kcfg, idx);
	c &= ~(0x03 << 7);
	if(ptt & (PTT_1 | PTT_2)) {
		if(ptt & PTT_1)
			c |= (1 << 6);
		if(ptt & PTT_2)
			c |= (1 << 7);
	}

	if(mode == MOD_CW)
		kcfg_set_val(kcfg, radio == RADIO_1 ? "r1Qsk" : "r2Qsk", (ptt & PTT_QSK) ? 1 : 0);

	kcfg_set_byte(kcfg, idx, c);
	return 0;
}
#endif

static unsigned char audiobyte_to_bits(unsigned char byte) {
	unsigned char res = 0x02;
	switch(byte) {
	case 'A':
		res = 0x02;
		break;
	case 'B':
		res = 0x01;
		break;
	case 'C':
		res = 0x03;
		break;
	case 'D':
		res = 0x00;
		break;
	default:
		warn("KCFG Invalid audio switch character: %c, using 'A' instead", byte);
		break;
	}
	return res;
}

static int conv_audiosw_ptt(const char *val, unsigned *out_result) {
	int i, len;
	unsigned char c, bits;

	if(val == NULL)
		return -1;

	len = strlen(val);

	/* Backward compatibility to 0xXX hex notation. */
	if(len > 2 && !strncmp(val, "0x", 2)) {
		sscanf(val, "%i", out_result);
		return 0;
	}

	if(len < 3)
		return -1;

	c = 0;

	for(i = 0; i < 3; i++) {
		bits = audiobyte_to_bits(val[i]);
		/* c &= ~(0x03 << (2*i)); */
		c |= (bits << (2*i));
	}

	dbg1("KCFG conversion phase 1 %s -> 0x%02x", val, c);

	if(len > 3 && val[3] == '1')
		c |= (1 << 6);
	if(len > 4 && val[4] == '1')
		c |= (1 << 7);

	dbg1("KCFG conversion phase 2 %s -> 0x%02x", val, c);

	*out_result = c;

	return 0;
}

int kcfg_apply_cfg(struct kcfg *kcfg, struct config *cfg) {
	const struct citem *cp;
	unsigned size;
	unsigned i;
	char key[256];
	unsigned val;

	cp = citems;
	size = ARRAY_SIZE(citems);

	for(i = 0; i < size; i++) {
		val = cp[i].def;
		snprintf(key, sizeof(key), "//Keyer/%s", cp[i].key);

		const char *s = cfg_get_str(cfg, key, NULL);

		/* Some unsexy special case handling. */
		if((!strcasecmp(cp[i].key, "r1FrBase") && !(kcfg->mhi->flags & MHF_HAS_FRBASE)) ||
		(!strncasecmp(cp[i].key, "r1FrBase_", 9) && !(kcfg->mhi->flags & MHF_HAS_MODE_FRBASE))) {
			if(s)
				warn("KCFG Ignoring option %s for %s", cp[i].key, kcfg->mhi->type_str);
			continue;
		}

		if(s == NULL) {
			kcfg_set_val(kcfg, cp[i].key, val);
			continue;
		}

		if(cp[i].conv_func) {
			if(0 != cp[i].conv_func(s, &val))
				err("KCFG Invalid value for %s: '%s'", cp[i].key, s);
		} else {
			sscanf(s, "%i", &val);
		}

		kcfg_set_val(kcfg, cp[i].key, val);
	}

	get_citem_ext(kcfg->mhi->type, &cp, &size);

	if(cp) {
		for(i = 0; i < size; i++) {
			snprintf(key, sizeof(key), "//Keyer/%s", cp[i].key);
			kcfg_set_val(kcfg, cp[i].key, cfg_get_int(cfg, key, cp[i].def));
		}
	}
	return 0;
}

struct kcfg *kcfg_create(const struct mh_info *mhi) {
	int size = 0;

	switch(mhi->type) {
	case MHT_CK:
	case MHT_DK:
	case MHT_MK:
		size = 12;
		break;
	case MHT_DK2:
	case MHT_MK2:
	case MHT_MK2R:
	case MHT_MK2Rp:
	case MHT_SM:
	case MHT_SMD:
	case MHT_U2R:
		size = 56;
		break;
	}

	if(!size) {
		err("KCFG Could not create. Unsupported keyer type!");
		return NULL;
	}

	struct kcfg *kcfg = w_calloc(1, sizeof(*kcfg));
	kcfg->mhi = mhi;
	buf_inc_size(&kcfg->b, size);
	return kcfg;
}

void kcfg_destroy(struct kcfg *kcfg) {
	if(kcfg)
		free(kcfg);
}

struct buffer *kcfg_get_buffer(struct kcfg *kcfg) {
	if(kcfg)
		return (&kcfg->b);
	return NULL;
}



void kcfg_test() {
/*
	struct mh_info mhi;
	mhi.type = MHT_DK2;
	struct kcfg *kcfg = kcfg_create(&mhi);

	kcfg_set_default(kcfg);

	dbg1("*** TEST ***");

	info("b1: 0x%0x", kcfg->b.data[1]);

	kcfg_set_val(kcfg, "sideTone", 7);
	info("b1: 0x%0x", kcfg->b.data[5]);

	kcfg_set_val(kcfg, "speedStep", 15);
	info("b1: 0x%0x", kcfg->b.data[5]);

	kcfg_set_val(kcfg, "speedStep", 0);
	info("b1: 0x%0x", kcfg->b.data[5]);

	kcfg_set_val(kcfg, "sideTone", 0);
	info("b1: 0x%0x", kcfg->b.data[5]);
*/
}
