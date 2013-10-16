/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "util.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1


void *w_calloc(size_t nmemb, size_t size) {
	void *p;
	do {
		p = calloc(nmemb, size);
		if(!p)
			sleep(1);
	} while(!p);
	return p;
}

void *w_malloc(size_t size) {
	void *p;
	do {
		p = malloc(size);
		if(!p)
			sleep(1);
	} while(!p);
	return p;
}

char *w_strdup(const char *src) {
	return strdup(src);

	char *n = w_malloc(strlen(src) + 1);
	strcpy(n, src);
	return n;
}

void *w_realloc (void *ptr, size_t size) {
	void *p;

	if(!size) {
		free(ptr);
		return NULL;
	}

	do {
		p = realloc(ptr, size);
		if(p == NULL)
			sleep(1);
	} while(!p);

	return p;
}



