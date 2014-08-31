/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef CITEM_H
#define CITEM_H

#include <stdint.h>

#define CITEM(key, idx, base_bit, width) { key, idx, base_bit, width, -1 }
#define CITEMD(key, idx, base_bit, width, def) { key, idx, base_bit, width, def }

struct citem {
	const char	*key;
	uint8_t		idx;
	uint8_t		base_bit;
	uint8_t		width;
	uint32_t	def;
};

struct cfg;

inline static uint8_t width2mask(int w) {
	return (0xff >> (8-w));
}


const struct citem *citem_find(const struct citem *citems,  uint16_t num_citems, const char *key);
int citem_get_value(const struct citem *citems, int num_citems, const uint8_t *buffer, int buffer_size, const char *key);
int citem_set_value(const struct citem *citems, int num_citems, uint8_t *buffer, int buffer_size, const char *key, int value);
int citems_to_cfg(struct cfg *cfg, const struct citem *citems, int num_citems, uint8_t *buffer, int buffer_size);
void citem_debug_print_values(const char *header, struct citem *citems, int num_citems, const uint8_t *buffer, int buffer_size);

#endif // CITEM_H
