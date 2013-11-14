/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHMK2_H
#define MHMK2_H

#include <stdint.h>

int mk2_get_mpk_value(const uint8_t mpk_buffer[4], const char *key);
void mk2_debug_print_mpk_values(const uint8_t mpk_buffer[4]);

#endif // MHMK2_H
