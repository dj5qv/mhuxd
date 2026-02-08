/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef KCFG_H
#define KCFG_H

#define KCFG_VALUE_MAX_SIZE (32)

struct mh_info;
struct config;

struct kcfg_iterator {
	struct kcfg *kcfg;
	int32_t idx;
};

struct kcfg *kcfg_create(const struct mh_info *mhi);
void kcfg_destroy(struct kcfg *kcfg);
struct buffer *kcfg_get_buffer(struct kcfg *kcfg);
int kcfg_set_val(struct kcfg *kcfg, const char *key, int val);

void kcfg_iter_begin(struct kcfg *kcfg, struct kcfg_iterator *iter);
int kcfg_iter_next(struct kcfg_iterator *iter);
int kcfg_iter_get(struct kcfg_iterator *iter, const char **keyp, int *valp);

void kcfg_update_keyer_mode(struct kcfg *kcfg, uint8_t cur, uint8_t r1, uint8_t r2);
void kcfg_update_mk1_frbase(struct kcfg *kcfg, int8_t mode);

#endif // KCFG_H
