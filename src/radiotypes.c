/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <string.h>
#include "radiotypes.h"
#include "global.h"

struct rig_type {
    unsigned char   key;
    unsigned char   icom_addr;
    const char     *name;
};

static const struct rig_type rig_types[] = {
    {0xFE , 0x00 , "none"},
    {0x44 , 0x00 , "Elecraft K2"},
    {0x62 , 0x00 , "Elecraft K3"},
    {0x75 , 0x00 , "Elecraft K3 (patched)"},
    {0x12 , 0x62 , "Icom IC-78"},
    {0x5D , 0x10 , "Icom IC-275E"},
    {0x77 , 0x14 , "Icom IC-475"},
    {0x28 , 0x68 , "Icom IC-703"},
    {0x02 , 0x48 , "Icom IC-706"},
    {0x03 , 0x4E , "Icom IC-706 MkII"},
    {0x04 , 0x58 , "Icom IC-706 MkII-G"},
    {0x24 , 0x3E , "Icom IC-707"},
    {0x48 , 0x5E , "Icom IC-718"},
    {0x05 , 0x28 , "Icom IC-725"},
    {0x06 , 0x30 , "Icom IC-726"},
    {0x07 , 0x38 , "Icom IC-728"},
    {0x25 , 0x3A , "Icom IC-729"},
    {0x08 , 0x04 , "Icom IC-735"},
    {0x09 , 0x40 , "Icom IC-736"},
    {0x26 , 0x3C , "Icom IC-737"},
    {0x27 , 0x44 , "Icom IC-738"},
    {0x0A , 0x56 , "Icom IC-746"},
    {0x0B , 0x66 , "Icom IC-746 Pro"},
    {0x0C , 0x50 , "Icom IC-756"},
    {0x0D , 0x5C , "Icom IC-756 Pro"},
    {0x0E , 0x64 , "Icom IC-756 ProII"},
    {0x4A , 0x6E , "Icom IC-756 ProIII"},
    {0x71 , 0x2C , "Icom IC-760 Pro"}, // ??
    {0x0F , 0x1E , "Icom IC-761"},
    {0x10 , 0x2C , "Icom IC-765"},
    {0x11 , 0x46 , "Icom IC-775"},
    {0x70 , 0x26 , "Icom IC-780"}, // ??
    {0x13 , 0x26 , "Icom IC-781"},
    {0x5A , 0x70 , "Icom IC-7000"},
    {0x74 , 0x76 , "Icom IC-7200"},
    {0x49 , 0x66 , "Icom IC-7400"},
    {0x76 , 0x7A , "Icom IC-7600"}, // ??
    {0x73 , 0x74 , "Icom IC-7700"},
    {0x4B , 0x6A , "Icom IC-7800"},
    {0x5C , 0x60 , "Icom IC-910H"},
    {0x5B , 0x00 , "JRC JST-145/245"},
    {0x4C , 0x00 , "Kenwood TS 140"},
    {0x15 , 0x00 , "Kenwood TS 440"},
    {0x16 , 0x00 , "Kenwood TS 450"},
    {0x47 , 0x00 , "Kenwood TS 480"},
    {0x1A , 0x00 , "Kenwood TS 570"},
    {0x4D , 0x00 , "Kenwood TS 680"},
    {0x17 , 0x00 , "Kenwood TS 690"},
    {0x4E , 0x00 , "Kenwood TS 711"},
    {0x4F , 0x00 , "Kenwood TS 790"},
    {0x50 , 0x00 , "Kenwood TS 811"},
    {0x18 , 0x00 , "Kenwood TS 850"},
    {0x1B , 0x00 , "Kenwood TS 870"},
    {0x51 , 0x00 , "Kenwood TS 940"},
    {0x19 , 0x00 , "Kenwood TS 950"},
    {0x1C , 0x00 , "Kenwood TS 2000"},
    {0x38 , 0x00 , "Ten-Tec OMNI VI"},
    {0x63 , 0x00 , "Ten-Tec OMNI VII"},
    {0x66 , 0x00 , "Ten-Tec OMNI VII (forced 56000 bps)"},
    {0x6F , 0x00 , "Ten-Tec OMNI VII (forced 57600 bps)"},
    {0x42 , 0x00 , "Ten-Tec ARGONAUT V"},
    {0x41 , 0x00 , "Ten-Tec JUPITER"},
    {0x40 , 0x00 , "Ten-Tec ORION"},
    {0x6E , 0x00 , "Ten-Tec ORION (forced 57600 bps)"},
    {0x5E , 0x00 , "Ten-Tec ORION II"},
    {0x5F , 0x00 , "Ten-Tec ORION II (forced 56000 bps)"},
    {0x6D , 0x00 , "Ten-Tec ORION II (forced 57600 bps)"},
    {0x64 , 0x00 , "Yaesu FT-450"},
    {0x45 , 0x00 , "Yaesu FT-757GXII"},
    {0x53 , 0x00 , "Yaesu FT-767"},
    {0x29 , 0x00 , "Yaesu FT-817"},
    {0x43 , 0x00 , "Yaesu FT-840"},
    {0x2A , 0x00 , "Yaesu FT-847"},
    {0x3D , 0x00 , "Yaesu FT-857"},
    {0x55 , 0x00 , "Yaesu FT-890"},
    {0x2B , 0x00 , "Yaesu FT-897"},
    {0x3A , 0x00 , "Yaesu FT-900"},
    {0x3B , 0x00 , "Yaesu FT-920"},
    {0x67 , 0x00 , "Yaesu FT-950"},
    {0x3C , 0x00 , "Yaesu FT-990"},
    {0x1F , 0x00 , "Yaesu FT-1000"},
    {0x20 , 0x00 , "Yaesu FT-1000D"},
    {0x21 , 0x00 , "Yaesu FT-1000MP"},
    {0x22 , 0x00 , "Yaesu FT-1000MP MkV"},
    {0x23 , 0x00 , "Yaesu FT-1000MP MkV Field"},
    {0x60 , 0x00 , "Yaesu FT-2000"},
    {0x72 , 0x00 , "Yaesu FT-2000D"},
    {0x79 , 0x00 , "Yaesu FT-5000"},
    {0x58 , 0x00 , "Yaesu FTdx9000D"},
    {0x68 , 0x00 , "Yaesu FTdx9000D (patched)"},
    {0x69 , 0x00 , "Yaesu FTdx9000MP"},
    {0x6A , 0x00 , "Yaesu FTdx9000MP (patched)"},
    {0x6B , 0x00 , "Yaesu FTdx9000 Contest"},
    {0x6C , 0x00 , "Yaesu FTdx9000 Contest (patched)"}
};

static int find_by_key(int key) {
	unsigned i;
	int idx = -1;
	for(i = 0; i < ARRAY_SIZE(rig_types); i++) {
		if(rig_types[i].key == key) {
			idx = i;
			break;
		}
	}
	return idx;
}

const char *rtyp_get_name(int key) {
	int idx = find_by_key(key);
	if(idx >= 0)
		return rig_types[idx].name;
	return "* Unknown key *";
}

void rtyp_print_types(FILE *f) {
	unsigned i;

	fprintf(f, "Code IcomAddr  Radio\n\n");
	for(i = 0; i < ARRAY_SIZE(rig_types); i++) {
		fprintf(f, "0x%02x   ", rig_types[i].key);
		if(strncmp(rig_types[i].name, "Icom", 4))
			fprintf(f, "       ");
		else
			fprintf(f, "0x%02x   ", rig_types[i].icom_addr);
		fprintf(f, "%s\n", rig_types[i].name);
	}
}
