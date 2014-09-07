/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2014  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef WEBUI_H
#define WEBUI_H 1

struct http_server;
struct cfgmgr;

struct webui * webui_create(struct http_server *hs, struct cfgmgr *cfgmgr);
void webui_destroy(struct webui *webui);


#endif /* WEBUI_H */
