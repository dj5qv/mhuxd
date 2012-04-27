/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHINFO_H
#define MHINFO_H

/* Supported microHam device types */

enum { MHT_UNKNOWN, MHT_CK, MHT_DK, MHT_DK2, MHT_MK, MHT_MK2,
       MHT_MK2R, MHT_MK2Rp, MHT_SM, MHT_SMD, MHT_U2R,  MHT_MAX };


/* Feature flags */

enum {
	MHF_HAS_CAT1 = (1<<0),
	MHF_HAS_CAT2 = (1<<1),
	MHF_HAS_CAT1_RADIO_SUPPORT = (1<<2),
	MHF_HAS_CAT2_RADIO_SUPPORT = (1<<3),
	MHF_HAS_AUX  = (1<<4),
	MHF_HAS_WINKEY = (1<<5),
	MHF_HAS_FSK1 = (1<<6),
	MHF_HAS_FSK2 = (1<<7),
	MHF_HAS_FRBASE = (1<<8),
	MHF_HAS_MODE_FRBASE = (1<<9),
};

struct mh_info {
	int    type;
	int    ver_fw_major;
	int    ver_fw_minor;
	int    ver_winkey;
	const char *type_str;
	int    flags;
};

struct mh_info_map {
	int             type;
	const char     *name;
	int             flags;
};

inline static int mhi_has_long_cat_setting(int type) {
	return (type == MHT_DK2 || type == MHT_MK2 || type == MHT_MK2R || type == MHT_MK2Rp || type == MHT_SM);
}

extern const struct mh_info_map mh_info_map[];
extern int mh_info_map_size;

#endif // MHINFO_H
