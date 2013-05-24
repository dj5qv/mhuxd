/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

void *w_calloc(size_t nmemb, size_t size);
void *w_malloc(size_t size);
char *w_strdup(const char *src);
void daemonize(void);

#endif // UTIL_H
