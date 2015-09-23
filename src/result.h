/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef RESULT_H
#define RESULT_H

#include <stdint.h>

struct result_message {
	struct PGNode node;
	char *text;
};

struct result {
	struct PGList msg_list;
	uint32_t result_code;
};


struct result *res_create();
void res_destroy(struct result *res);
void res_msg_add(struct result *res, const char *text);
void res_msg_add_f(struct result *res, const char *fmt, ...) ;


#endif /* RESULT_H */
