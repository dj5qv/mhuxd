/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 *  Clearsilver headers include ClearSilver's config.h (cs_config.h).
 *  That contains the standard autotools macros like PACKAGE_STRING etc.
 *  That is conflicting with our config.h.
 *  So here we go for a workaround..
 *
 */

#include "config.h"

const char *_package = PACKAGE;
const char *_package_version = PACKAGE_VERSION;
