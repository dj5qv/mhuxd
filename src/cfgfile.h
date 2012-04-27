/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CFGFILE_H
#define CFGFILE_H

#include "pglist.h"

struct config *cfg_load(const char *filename);
void cfg_destroy(struct config *c);
const char* cfg_get_str(struct config *c, const char *subkey, const char *def);
int cfg_get_bool(struct config *c, const char *subkey, int def);
int cfg_get_int(struct config *c, const char *key, int def);
float cfg_get_float(struct config *c, const char *key, float def);

#endif // CFGFILE_H
