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

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#define STR(tok) #tok
#define STRINGIFY(tok) STR(tok)

void *w_calloc(size_t nmemb, size_t size);
void *w_malloc(size_t size);
char *w_strdup(const char *src);
void *w_realloc (void *ptr, size_t size);

#endif // UTIL_H
