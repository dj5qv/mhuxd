/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include "cfgfile.h"
#include "util.h"
#include "logger.h"

#define SEPC '/'
#define SEPS "/"


struct config {
	struct PGList items;
	struct item *cur_item;
	char    *cur_path;
	unsigned cur_line;
};

struct item {
	struct	PGNode node;
	char *key;
	char *val;
	int	is_group;
};

static char *realloc_append(char *dest, char *src) {
	char *new = w_malloc(strlen(dest) + strlen(src) + 1);
	*new = 0x00;
	strcat(new, dest);
	strcat(new, src);
	free(dest);
	return new;
}

static void parse_pair(const char *src, char **key, char **val) {
	const char *p;
	const char *vstart;
	int size;

	p = src;
	while(*p && !isspace(*p) && *p != '>' && *p != '\n')
		p++;

	size = p - src;
	if(!size)
		return;
	
	*key = w_calloc(1, size + 1);
	memcpy(*key, src, size);

	while(isspace(*p))
		p++;

	if(!*p || *p == '>' || *p == '\n')
		return;

	vstart = p;

	while(*p && !isspace(*p) && *p != '>' && *p != '\n')
		p++;

	size = p - vstart;
	if(!size)
		return;

	*val = w_calloc(1, size + 1);
	memcpy(*val, vstart, size);
}

static void parse_line(struct config *c, const char *line) {
	struct item *it;
	const char *p; 
	const char *pe;
	char *key;
	char *val;
	int  is_group;
	
	is_group = 0;
	key = NULL;
	val = NULL;

	p = line;

	while(isspace(*p))
		p++;

	if(!isprint(*p) || *p == '#')
		return;

	if(*p == '<') {
		pe = strstr(p, ">") ;
		if(!pe) {
			err("CFGFILE Error in line %d", c->cur_line);
			return;
		}
		is_group = 1;
		p++;
		while(isspace(*p))
			p++;
	}
	parse_pair(p, &key, &val);

	if(!key)
		return;

	if(*key == '/') {
		char *pt = strrchr(c->cur_path, SEPC);
		if(pt)
			*pt = 0x00;
		else
			err("CFGFILE Error in line %d", c->cur_line);
		free(key);
		if(val)
			free(val);
		return;
	}

	if(is_group && val) {
		key = realloc_append(key, ":");
		key = realloc_append(key, val);
	}

	it = w_calloc(1, sizeof(*it));
	it->key = w_malloc(strlen(c->cur_path) + strlen(key) + 2);
	strcpy(it->key, c->cur_path);
	strcat(it->key, SEPS);
	strcat(it->key, key);
	it->val = val;
	it->is_group = is_group;

	if(is_group) {
		free(c->cur_path);
		c->cur_path = strdup(it->key);
	}

	dbg0("CFGFILE added %s: %s", it->key, it->val ? it->val : "");
	PG_AddTail(&c->items, &it->node);
	free(key);
}


struct config *cfg_load(const char *filename) {
	struct config *c;
	FILE *f;
	char *line;
	size_t size = 0;

	if(!filename)
		return NULL;

	f = fopen(filename, "r");
	if(!f) {
		err_e(-errno, "CFGFILE Could not open config file %s!", filename);
		return NULL;
	}

	c = w_calloc(1, sizeof(*c));
	PG_NewList(&c->items);
	c->cur_path = strdup("/");

	line = NULL;
	while(-1 != getline(&line, &size, f)) {
		c->cur_line++;
		parse_line(c, line);
	}

	if(line)
		free(line);

	free(c->cur_path);
	c->cur_path = NULL;
	fclose(f);
	return c;
}

void cfg_destroy(struct config *c) {
	struct item *it;
	while((it = (void *)PG_FIRSTENTRY(&c->items))) {
		if(it->val)
			free(it->val);
		if(it->key)
			free(it->key);
		it->key = it->val = NULL;
		PG_Remove(&it->node);
		free(it);
	}
	free(c);
}

const char* cfg_get_str(struct config *c, const char *key, const char *def) {
	struct item *it;
	int found = 0;

	if(!c || !key || PG_LISTEMPTY(&c->items))
		return def;

	PG_SCANLIST(&c->items, it) {
		if(!strcasecmp(it->key, key)) {
			found = 1;
			break;
		}
	}
	return found ? it->val : def;
}

int cfg_get_bool(struct config *c, const char *key, int def) {
	const char *val;
	val = cfg_get_str(c, key, NULL);
	if(!val)
		return def;

	if(!strcasecmp(val, "on") || !strcmp(val, "1"))
		return 1;
	if(!strcasecmp(val, "off") || !strcmp(val, "0"))
		return 0;
	warn("CFG Invalid value %s for %s", val, key);
	return def;
}

int cfg_get_int(struct config *c, const char *key, int def) {
	int res = def;
	const char *val;

	val = cfg_get_str(c, key, NULL);
	if(!val)
		return def;

	if(!strcasecmp(val, "on"))
		return 1;
	if(!strcasecmp(val, "off"))
		return 0;

	sscanf(val, "%i", &res);

	return res;
}

float cfg_get_float(struct config *c, const char *key, float def) {
	float res = def;
	const char *val;
	val = cfg_get_str(c, key, NULL);
	if(!val)
		return def;

	sscanf(val, "%f", &res);
	return res;
}
