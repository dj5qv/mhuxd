/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "util.h"
#include "pglist.h"
#include "result.h"

struct result *res_create() {
	struct result *res = w_calloc(1, sizeof(*res));
	PG_NewList(&res->msg_list);

	return res;
}

void res_destroy(struct result *res) {
	struct PGNode *node;

	while((node = PG_FIRSTENTRY(&res->msg_list))) {
		PG_Remove(node);
		free(node);
	}
	free(res);
}

void res_msg_add(struct result *res, const char *text) {
	struct result_message *msg;
	size_t len;

	len = strlen(text);
	msg = w_calloc(1, sizeof(*msg) + len + 1);
	msg->text = (void*)&msg[1];
	memcpy(msg->text, text, len);
	PG_AddTail(&res->msg_list, &msg->node);
}

void res_msg_add_f(struct result *res, const char *fmt, ...) {
	va_list ap;
	char *p;
	int size = 100, n;
	
	p = w_malloc(size);
	
	while(1) {
		/* Try to print in the allocated space. */

		va_start(ap, fmt);
		n = vsnprintf(p, size, fmt, ap);
		va_end(ap);

		if (n > -1 && n < size) {
			res_msg_add(res, p);
			return;
		}

		if (n > -1)    /* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		else           /* glibc 2.0 */
			size *= 2;  /* twice the old size */

		p = w_realloc(p, size);
	}
}
