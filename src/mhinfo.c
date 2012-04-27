/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include "mhinfo.h"
#include "global.h"

const struct mh_info_map mh_info_map[] = {
	{ MHT_UNKNOWN, "Unkown", 0 },

	{ MHT_CK,      "CW KEYER",
	  MHF_HAS_WINKEY },

	{ MHT_DK,      "DIGI KEYER",
	  MHF_HAS_CAT1|MHF_HAS_FSK1 },

	{ MHT_DK2,     "DIGI KEYER II",
	  MHF_HAS_CAT1|MHF_HAS_CAT1_RADIO_SUPPORT|MHF_HAS_WINKEY|MHF_HAS_FSK1},

	{ MHT_MK,      "micro KEYER",
	  MHF_HAS_CAT1|MHF_HAS_WINKEY|MHF_HAS_FRBASE|MHF_HAS_FSK1 },

	{ MHT_MK2,     "micro KEYER II",
	  MHF_HAS_CAT1|MHF_HAS_CAT1_RADIO_SUPPORT|MHF_HAS_AUX|MHF_HAS_WINKEY|MHF_HAS_MODE_FRBASE|MHF_HAS_FSK1 },

	{ MHT_MK2R,    "MK2R",
	  MHF_HAS_CAT1|MHF_HAS_CAT1_RADIO_SUPPORT|MHF_HAS_CAT2|
	  MHF_HAS_CAT2_RADIO_SUPPORT|MHF_HAS_WINKEY|MHF_HAS_MODE_FRBASE|MHF_HAS_FSK1|MHF_HAS_FSK2 },

	{ MHT_MK2Rp,   "MK2R+",
	  MHF_HAS_CAT1|MHF_HAS_CAT1_RADIO_SUPPORT|MHF_HAS_CAT2|
	  MHF_HAS_CAT2_RADIO_SUPPORT|MHF_HAS_WINKEY|MHF_HAS_MODE_FRBASE|MHF_HAS_FSK1|MHF_HAS_FSK2 },

	{ MHT_SM,      "Station Master",
	  MHF_HAS_CAT1|MHF_HAS_CAT1_RADIO_SUPPORT|MHF_HAS_AUX },

	{ MHT_SMD,     "Station Master DeLuxe",
	  MHF_HAS_CAT1|MHF_HAS_CAT1_RADIO_SUPPORT|MHF_HAS_AUX },

	{ MHT_U2R,     "micro2R", MHF_HAS_WINKEY|MHF_HAS_FSK1|MHF_HAS_FSK2 }
};

int mh_info_map_size = ARRAY_SIZE(mh_info_map);

