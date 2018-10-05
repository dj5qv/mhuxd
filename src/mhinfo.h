/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHINFO_H
#define MHINFO_H

#include <stdint.h>

/* Supported microHam device types */

enum { MHT_UNKNOWN, MHT_CK, MHT_DK, MHT_DK2, MHT_MK, MHT_MK2,
       MHT_MK2R, MHT_MK2Rp, MHT_SM, MHT_SMD, MHT_U2R, MHT_MK3, MHT_MAX };


/* Feature flags */

enum {
	MHF_HAS_R1 = (1<<0),
	MHF_HAS_R2 = (1<<1),
	MHF_HAS_R1_RADIO_SUPPORT = (1<<2),
	MHF_HAS_R2_RADIO_SUPPORT = (1<<3),
	MHF_HAS_AUX  = (1<<4),
	MHF_HAS_WINKEY = (1<<5),
	MHF_HAS_FSK1 = (1<<6),
	MHF_HAS_FSK2 = (1<<7),
	MHF_HAS_FRBASE = (1<<8),
	MHF_HAS_FRBASE_CW = (1<<9),
	MHF_HAS_FRBASE_DIGITAL = (1<<10),
	MHF_HAS_FRBASE_VOICE = (1<<11),

	MHF_HAS_LNA_PA_PTT = (1<<12),
	MHF_HAS_LNA_PA_PTT_TAIL = (1<<13),
	MHF_HAS_SOUNDCARD_PTT = (1<<14),
	MHF_HAS_CW_IN_VOICE = (1<<15),

	MHF_HAS_AUDIO_SWITCHING = (1<<16),
	MHF_HAS_DISPLAY = (1<<17),
	MHF_HAS_FOLLOW_TX_MODE = (1<<18),

	MHF_HAS_PTT_SETTINGS = (1<<19),
	MHF_HAS_KEYER_MODE = (1<<20),
	MHF_HAS_FLAGS_CHANNEL = (1<<21),
	MHF_HAS_MCP_SUPPORT = (1<<22),
	MHF_HAS_ROTATOR_SUPPORT = (1<<23),
	MHF_HAS_SM_COMMANDS = (1<<24),

	MHF_HAS_PFSK = (1<<25),
	MHF_HAS_PCW = (1<<26),

	MHF_MHUXD_SUPPORTED = (1<<31)

};

struct mh_info {
	uint16_t type;
	uint16_t ver_fw_major;
	uint16_t ver_fw_minor;
	uint16_t ver_winkey;
	uint32_t flags;
	const char *type_str;
};

struct mh_info_map {
	const char  *name;
	uint32_t    flags;
	uint16_t    type;
};

void mhi_init(struct mh_info *mhi, int type);
int mhi_parse_version(struct mh_info *mhi, const uint8_t *data, uint16_t len);
int mhi_type_from_serial(const char *serial);

#endif // MHINFO_H
