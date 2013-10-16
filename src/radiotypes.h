/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef RADIOTYPES_H
#define RADIOTYPES_H

#include <stdint.h>

struct rig_type {
    unsigned char   key;
    unsigned char   icom_addr;
    const char     *name;
};

extern const int num_rig_types;
extern const struct rig_type rig_types[];

const char *rtyp_get_name(int key);
uint8_t rtyp_get_icom_address(int key);

#endif // RADIOTYPES_H
