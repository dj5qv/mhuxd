/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <string.h>
#include "citem.h"
#include "logger.h"
#include "cfgnod.h"

const struct citem *citem_find(const struct citem *citems,  uint16_t num_citems, const char *key) {
	uint16_t i;

	for(i = 0; i < num_citems; i++) {
		if(!strcmp(key, citems[i].key))
			return &citems[i];
	}
	return NULL;
}

int citem_get_value(const struct citem *citems, int num_citems, const uint8_t *buffer, int buffer_size, const char *key) {

	const struct citem *cp = citem_find(citems, num_citems, key);
	if(!cp)
		return -1;

	uint16_t idx = cp->off + cp->base_bit / 8;
	uint16_t bit = cp->base_bit % 8;
	uint16_t mask = width2mask(cp->width);

	if(idx >= buffer_size) {
		err("(citem) %s() idx %d out of range for key '%s'!", __func__, idx, key);
		return -1;
	}

	return (buffer[idx] >> (bit + 1 - cp->width)) & mask;
}

int citem_set_value(const struct citem *citems, int num_citems, uint8_t *buffer, int buffer_size, const char *key, int value) {
	const struct citem *cp;
	int c;
	int idx, bit, mask;

	cp = citem_find(citems, num_citems, key);
	if(!cp) {
		err("(citem) %s() unknown option: %s", __func__,  key);
		return -1;
	}

	idx = cp->off + cp->base_bit / 8;
	bit = cp->base_bit % 8;
	mask = width2mask(cp->width);

	if(idx >= buffer_size) {
		err("(citem) %s() index out of range for %s", __func__, key);
	}

	c = buffer[idx];

	if(value > mask) {
		err("(citem) %s() invalid value %d for %s", __func__, value, key);
		return -1;
	}

	c &= ~(mask << (bit + 1 - cp->width));
	c |=  (value << (bit + 1 - cp->width));

	buffer[idx] = c;

	return 0;

}

void citem_debug_print_values(const char *header, struct citem *citems, int num_citems, const uint8_t *buffer, int buffer_size) {
	uint16_t i;

	if(log_get_level() < LOGSV_DBG1)
		return;

	for(i = 0; i < num_citems; i++) {
		dbg1("(citem) %s %s: %d",
		     header,
		     citems[i].key,
		     citem_get_value(citems, num_citems, buffer, buffer_size, citems[i].key));
	}
}

int citems_to_cfg(struct cfg *cfg, const struct citem *citems, int num_citems, uint8_t *buffer, int buffer_size) {
	const struct citem *cp;
	int c;
	uint16_t idx, bit, mask, i;

	for(i = 0; i < num_citems; i++) {
		cp = &citems[i];
		idx = cp->off + cp->base_bit / 8;
		bit = cp->base_bit % 8;
		mask = width2mask(cp->width);

		if(idx >= buffer_size) {
			err("(citem) %s() index out of range for %s", __func__, cp->key);
			return -1;
		}

		c = buffer[idx];

		cfg_set_int_value(cfg, citems[i].key, (c >> (bit + 1 - cp->width)) & mask);
	}
	return 0;
}