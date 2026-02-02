/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef CFGMGRJ_H
#define CFGMGRJ_H 1

struct ev_loop;
struct cfgmgrj;

struct cfgmgrj *cfgmgrj_create(struct ev_loop *loop);
void cfgmgrj_destroy(struct cfgmgrj *cfgmgrj);
int cfgmgrj_load_cfg(struct cfgmgrj *cfgmgrj);
int cfgmgrj_save_cfg(struct cfgmgrj *cfgmgrj);

#endif // CFGMGRJ_H
