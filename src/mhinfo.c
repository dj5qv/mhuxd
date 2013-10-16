/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdlib.h>
#include <string.h>
#include "mhinfo.h"
#include "util.h"

const struct mh_info_map mh_info_map[] = {
	{ .type = MHT_UNKNOWN, 
	  .name = "Unknown", 
	  .flags = 0 },

	{ .type = MHT_CK,       
	  .name = "CW KEYER",
	  .flags = MHF_HAS_WINKEY },

	{ .type = MHT_DK,
	  .name = "DIGI KEYER",
	  .flags = MHF_HAS_R1|MHF_HAS_FSK1|MHF_HAS_LNA_PA_PTT},

	{ .type = MHT_DK2, 
	  .name = "DIGI KEYER II",
	  .flags = MHF_HAS_R1|MHF_HAS_R1_RADIO_SUPPORT|MHF_HAS_WINKEY|MHF_HAS_FSK1|MHF_HAS_FRBASE_CW|MHF_HAS_FRBASE_DIGITAL|
		MHF_HAS_LNA_PA_PTT|MHF_HAS_LNA_PA_PTT_TAIL|MHF_HAS_SOUNDCARD_PTT|MHF_HAS_FOLLOW_TX_MODE },

	{ .type = MHT_MK,      
	  .name = "micro KEYER",
	  .flags = MHF_HAS_R1|MHF_HAS_WINKEY|MHF_HAS_FRBASE|MHF_HAS_FSK1|MHF_HAS_LNA_PA_PTT|MHF_HAS_AUDIO_SWITCHING },

	{ .type = MHT_MK2,     
	  .name = "micro KEYER II",
	  .flags = MHF_HAS_R1|MHF_HAS_R1_RADIO_SUPPORT|MHF_HAS_AUX|MHF_HAS_WINKEY|MHF_HAS_FRBASE_VOICE|
	  MHF_HAS_FRBASE_DIGITAL|MHF_HAS_FRBASE_CW|MHF_HAS_FSK1|MHF_HAS_LNA_PA_PTT|
	  MHF_HAS_LNA_PA_PTT_TAIL|MHF_HAS_SOUNDCARD_PTT|MHF_HAS_CW_IN_VOICE|MHF_HAS_AUDIO_SWITCHING|
	  MHF_HAS_DISPLAY|MHF_HAS_FOLLOW_TX_MODE},

	{ .type = MHT_MK2R,    
	  .name = "MK2R",
	  .flags = MHF_HAS_R1|MHF_HAS_R1_RADIO_SUPPORT|MHF_HAS_R2|
		MHF_HAS_R2_RADIO_SUPPORT|MHF_HAS_WINKEY|MHF_HAS_FRBASE_VOICE|
		MHF_HAS_FRBASE_DIGITAL|MHF_HAS_FRBASE_CW|MHF_HAS_FSK1|MHF_HAS_FSK2|
		MHF_HAS_LNA_PA_PTT|
		MHF_HAS_LNA_PA_PTT_TAIL|MHF_HAS_CW_IN_VOICE|MHF_HAS_AUDIO_SWITCHING|MHF_HAS_FOLLOW_TX_MODE },

	{ .type = MHT_MK2Rp,   
	  .name = "MK2R+",
	  .flags = MHF_HAS_R1|MHF_HAS_R1_RADIO_SUPPORT|MHF_HAS_R2|
		MHF_HAS_R2_RADIO_SUPPORT|MHF_HAS_WINKEY|MHF_HAS_FRBASE_VOICE|
		MHF_HAS_FRBASE_DIGITAL|MHF_HAS_FRBASE_CW|MHF_HAS_FSK1|MHF_HAS_FSK2|
		MHF_HAS_LNA_PA_PTT|
		MHF_HAS_LNA_PA_PTT_TAIL|MHF_HAS_CW_IN_VOICE|MHF_HAS_AUDIO_SWITCHING|MHF_HAS_FOLLOW_TX_MODE},

	{ .type = MHT_SM,      
	  .name = "Station Master",
	  .flags = MHF_HAS_R1|MHF_HAS_R1_RADIO_SUPPORT|MHF_HAS_AUX  },

	{ .type = MHT_SMD,     
	  .name = "Station Master DeLuxe",
	  .flags = MHF_HAS_R1|MHF_HAS_R1_RADIO_SUPPORT|MHF_HAS_AUX },

	{ .type = MHT_U2R,     
	  .name = "micro2R", 
	  .flags = MHF_HAS_WINKEY|MHF_HAS_FSK1|MHF_HAS_FSK2|MHF_HAS_AUDIO_SWITCHING }
};

int mh_info_map_size = ARRAY_SIZE(mh_info_map);

int mhi_parse_version(struct mh_info *mhi, const uint8_t *data, uint16_t len) {
	const uint8_t *p;

	mhi->type = MHT_UNKNOWN;

	if(data == NULL || len < 19)
		return -1;

	p = data;
	p++;

	int t = (p[2] << 8) | p[5];
	switch(t) {
	case 0x1000:
		switch(p[17]) {
		case 0x0f:
			mhi->type = MHT_DK;
			break;
		case 0xf0:
			mhi->type = MHT_CK;
			break;
		default:
			mhi->type = MHT_MK;
			break;
		}
		break;
	case 0x1201:
		mhi->type = MHT_MK2;
		break;
	case 0x1601:
		mhi->type = MHT_DK2;
		break;
	case 0x1101:
		mhi->type = MHT_MK2R;
		break;
	case 0x1102:
		mhi->type = MHT_MK2Rp;
		break;
	case 0x1501:
		mhi->type = MHT_U2R;
		break;
	case 0x1301:
		mhi->type = MHT_SM;
		break;
	case 0x1401:
		mhi->type = MHT_SMD;
		break;
	default:
		mhi->type = MHT_UNKNOWN;
		break;
	}

	mhi->ver_winkey = p[16];
	mhi->ver_fw_major = p[9];
	mhi->ver_fw_minor = p[10];

	int i;
	for(i = 0; i < mh_info_map_size; i++) {
		if(mhi->type == mh_info_map[i].type) {
			mhi->type_str = mh_info_map[i].name;
			mhi->flags = mh_info_map[i].flags;
			break;
		}
	}
	return 0;
}

void mhi_init(struct mh_info *mhi, int type) {
	int i;

	memset(mhi, 0, sizeof(*mhi));
	mhi->type = type;

	mhi->type_str = "Unknown";

	for(i = 0; i < mh_info_map_size; i++) {
		if(mhi->type == mh_info_map[i].type) {
			mhi->type_str = mh_info_map[i].name;
			mhi->flags = mh_info_map[i].flags;
			break;
		}
	}
}

